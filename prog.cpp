#include <iostream>
#include <cstring>
#include <fstream>
#include <string>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <ctime>

#define MAX_XFER_BUF_SIZE 16384

using namespace std;

ssh_session ssh;
sftp_session sftp;
sqlite3 *db;

void sql_connect(char user[], char password[], char db_name[]){
    char sql_create_table[] = "CREATE TABLE IF NOT EXISTS sftp_copy(file_name, time);";
    char *errmsg;
    if(sqlite3_open(db_name, &db)){
        cout << "Ошибка SQL: " << sqlite3_errmsg(db) << endl;
        exit(0);
    }else if(sqlite3_exec(db, sql_create_table, 0, 0, &errmsg)){                        // создание таблицы
        cout << "Ошибка SQL: " << errmsg << endl;
        exit(0);
    }
}

void settings_file_format(){
    cout << endl << "sftp_host=127.0.0.1" << endl;
    cout << "sftp_port=22" << endl;
    cout << "sftp_user=user" << endl;
    cout << "sftp_password=password" << endl;
    cout << "sftp_remote_dir=/path/to/remotedir" << endl;
    cout << "local_dir=/path/to/localdir" << endl;
    cout << "sql_user=user_name" << endl;
    cout << "sql_password=password" << endl;
    cout << "sql_database=database_name" << endl << endl;
}

void connect_to_ssh(char host[], char port[], char user[], char password[]){
    cout << endl << "Подключение..." << endl;
    int check_connect;
    ssh = ssh_new();
    ssh_options_set(ssh, SSH_OPTIONS_HOST, host);
    check_connect = ssh_connect(ssh);
    if(check_connect != SSH_OK){
        cout  << ssh_get_error(ssh) << endl;
        exit(0);
    }else{
        cout << "Подключено к " << host << endl;
    }
    cout << "Пользователь: " << user << endl;
    cout << "Пароль: ";
    for(int i = 1; i <= strlen(password); i++)
        cout << "*";
    cout << endl;
    check_connect = ssh_userauth_password(ssh, user, password);
    if(check_connect != SSH_AUTH_SUCCESS){
        cout << ssh_get_error(ssh);
        exit(0);
    }else{
        cout << ">>Удачно<<" << endl;
    }
    sftp = sftp_new(ssh);
    if(sftp_init(sftp)){
        cout << "Ошибка инициализации sftp: " << ssh_get_error(ssh) << endl;
        exit(0);
    }
}

void sftp_copy(char path[], char ldir[]){
    time_t now;
    char *time_now;
    tm *ltm;
    char sql_record[106];
    char *errmsg;

    char lfile[strlen(ldir) + 30];              //путь к локальному файлу
    char ofile[strlen(path) + 30];              //путь к удаленному файлу
    char check;

    sftp_dir dir;
    sftp_attributes read_file;                 //для чтения имён файлов из dir
    sftp_file file;                            //файл для считывания

    dir = sftp_opendir(sftp, path);
    if(!dir) {
        cout << "Удалённая директория не открыта: " << ssh_get_error(ssh) << endl;
        exit(0);
    }
    cout << endl << "Файлы в удалённой дирректории: " << endl;
    while((read_file = sftp_readdir(sftp,dir)) && strcmp(read_file->name, "..")){
        cout << read_file->name << endl;
        sftp_attributes_free(read_file);
    }
    cout << endl << "Соответствует?" << endl;
    do{
        cout << "y/n >> ";
        cin >> check;
    }while(check != 'n' && check != 'N' && check != 'y' && check != 'Y');
    if(check == 'n' || check == 'N'){
        cout << "Формат записи файла настроек:" << endl;
        settings_file_format();
        exit(0);
    }
    sftp_closedir(dir);
    dir = sftp_opendir(sftp, path);
    char buffer[MAX_XFER_BUF_SIZE];
    int buf_size = 0;
    cout << endl << "Копирование: " << endl;
    while((read_file = sftp_readdir(sftp,dir)) && strcmp(read_file->name, "..")){
        strcpy(sql_record, "INSERT INTO sftp_copy(file_name, time) VALUES('");
        strcat(sql_record, read_file->name);
        strcat(sql_record, "','");
        cout << path << "/" << read_file->name << " --> " << ldir << "/" << read_file->name << endl;
        strcpy(lfile, ldir);
        strcpy(ofile, path);
        if(ofile[strlen(ofile)] != '/')
            strcat(ofile, "/");
        strcat(ofile, read_file->name);
        file = sftp_open(sftp, ofile, O_RDONLY, 0);
        if(lfile[strlen(lfile)] != '/')
            strcat(lfile, "/");
        strcat(lfile, read_file->name);
        ofstream new_file;
        new_file.open(lfile, ios::binary);
        while(1){
            buf_size = sftp_read(file, buffer, sizeof(buffer));
            if (buf_size == 0){
                break;
            }
            else if (buf_size < 0){
                cout << "Ошибка чтения: " <<  ssh_get_error(ssh) << endl;
                sftp_close(file);
                exit(0);
            }
            new_file.write(buffer, buf_size);
            if (!new_file){
                cout << "Ошибка записи" << endl;
                sftp_close(file);
                exit(0);
            }
        }
        now = time(NULL);
        ltm = localtime(&now);
        time_now = asctime(ltm);
        strcat(sql_record, time_now + 4);
        strcat(sql_record, "');");
        if(sqlite3_exec(db, sql_record, 0, 0, &errmsg)){
            cout << "Ошибка SQL: " << errmsg << endl;
            exit(0);
        }
    }
}


static int callback(void *data, int argc, char **argv, char **azColName){   //для вывода из БД
    cout << azColName[0] << "▎ " << argv[0] << endl;
    cout << azColName[1];
    for(int i = 0; i <= strlen(azColName[0]) - strlen(azColName[1]) - 1; i++)
        cout << " "; 
    cout << "▎ " << argv[1];
    for(int i = 0; i <= strlen(azColName[0]) + strlen(argv[1]); i++)
        cout << "▰";
    cout << endl;
    return 0;
}

void sql_out(){
    char SQL[] = "SELECT * FROM sftp_copy;";
    char *data;
    char *errmsg;

    cout << endl << "История копирования файлов:" << endl << endl;
    if(sqlite3_exec(db, SQL, callback, (void*)data, &errmsg)){
        cout << "Ошибка SQL:" << errmsg << endl;
        exit(0);
    }
}

int main(int argc, char* argv[]){
    setlocale(LC_ALL, "rus");
    if(argv[1] == nullptr){
        cout << "Введите путь к файлу с настройками" << endl;
        return 0;
    }
    int length = 0;
    for(int i = 1; i < argc; i++){                          //установка размера char path[]
        length += strlen(argv[i]);
    }
    char path[length] = "";
    for(int i = 1; i < argc; i++){                          //если путь с пробелами
        if(i != 1)
            strcat(path, " ");
        strcat(path, argv[i]);
    }   
    ifstream set_file;
    set_file.open(path);
    if(!set_file.is_open()){
        cout << "Файла не существует!" << endl;
        return 0;    
    }            
    cout << "Файл с настройками получен..." << endl;
    char check;
    if(!set_file.get(check)){
        cout << "В файле ничего нет!!!" << endl;
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    string str;
    int i = 1;
    str = "";
    getline(set_file, str);                     //достаём хост
    if(str[8] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 9; str[j] != '\0'; j++){
        length++;        
    }
    char sftp_host[length];
    for(int j = 0; j <= length; j++){
        sftp_host[j] = str[j+9];
    }
    cout << "sftp_host >> " << sftp_host << endl;
    str = "";
    getline(set_file, str);                  //достаём порт
    if(str[9] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 10; str[j] != '\0'; j++){
        length++;        
    }
    char sftp_port[length];
    for(int j = 0; j <= length; j++){
        sftp_port[j] = str[j+10];
    }
    cout << "sftp_port >> " << sftp_port << endl;
    str = "";                               
    getline(set_file, str);                 //достаём sftp_user
    if(str[9] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 10; str[j] != '\0'; j++){
        length++;        
    }
    char sftp_user[length];
    for(int j = 0; j <= length; j++){
        sftp_user[j] = str[j+10];
    }
    cout <<  "sftp_user >> " << sftp_user << endl;
    str = "";                               
    getline(set_file, str);                 //достаём sftp_password
    if(str[13] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 13; str[j] != '\0'; j++){
        length++;        
    }
    char sftp_password[length];
    for(int j = 0; j <= length; j++){
        sftp_password[j] = str[j+14];
    }
    cout <<  "sftp_password >> " << sftp_password[0];
    for(int i = 1; i <= strlen(sftp_password) - 2; i++)
        cout << "*";
    cout << sftp_password[length-2] << endl;
    str = "";                               
    getline(set_file, str);                 //достаём sftp__remote_dir
    if(str[15] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 15; str[j] != '\0'; j++){
        length++;        
    }
    char sftp_remote_dir[length];
    for(int j = 0; j <= length; j++){
        sftp_remote_dir[j] = str[j+16];
    }
    cout <<  "sftp_remote_dir >> " << sftp_remote_dir << endl;   
    str = "";                               
    getline(set_file, str);                 //достаём local_dir
    if(str[9] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 10; str[j] != '\0'; j++){
        length++;        
    }
    char local_dir[length];
    for(int j = 0; j <= length; j++){
        local_dir[j] = str[j+10];
    }
    cout <<  "local_dir >> " << local_dir << endl; 
    str = "";                               
    getline(set_file, str);                 //достаём sql_user
    if(str[8] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 9; str[j] != '\0'; j++){
        length++;        
    }
    char sql_user[length];
    for(int j = 0; j <= length; j++){
        sql_user[j] = str[j+9];
    }
    cout <<  "sql_user >> " << sql_user << endl;
    str = "";                               
    getline(set_file, str);                 //достаём sql_password
    if(str[12] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 13; str[j] != '\0'; j++){
        length++;        
    }
    char sql_password[length];
    for(int j = 0; j <= length; j++){
        sql_password[j] = str[j+13];
    }
    //cout <<  "sql_password >> " << sql_password << endl;
    cout <<  "sql_password >> " << sql_password[0];
    for(int i = 1; i <= strlen(sql_password) - 2; i++)
        cout << "*";
    cout << sql_password[length-1] << endl;
    str = "";                               
    getline(set_file, str);                 //достаём sql_password
    if(str[12] != '='){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    length = 0;
    for(int j = 13; str[j] != '\0'; j++){
        length++;        
    }
    char sql_database[length];
    for(int j = 0; j <= length; j++){
        sql_database[j] = str[j+13];
    }
    cout <<  "sql_database >> " << sql_database << endl;
    set_file.close();
    cout << endl << "Всё правильно?" << endl;
    do{
        cout << "y/n >> ";
        cin >> check;
    }while(check != 'n' && check != 'N' && check != 'y' && check != 'Y');
    if(check == 'n' || check == 'N'){
        cout << "Формат записи:" << endl;
        settings_file_format();
        return 0;
    }
    sql_connect(sql_user, sql_password, sql_database);
    connect_to_ssh(sftp_host, sftp_port, sftp_user, sftp_password);
    sftp_copy(sftp_remote_dir, local_dir);
    sql_out();
    cout << endl << "Завершено успешно" << endl;
    sqlite3_close(db);
    return 1;
}