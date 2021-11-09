Стракович Юрий Александрович
____
# Задание 1

+ Запуск через консоль
+ Единственный параметр (параметр передаваемый через консоль при запуске приложения) - путь к файлу с настройками
+ После запуска приложение копирует все файлы по SFTP (параметры подключения берутся из файла настройки пункта #2 из удаленной директории (sftp_remote_dir) в локальную (local_dir)
+ В локальную базу данных (MySQL или SQLite на выбор - параметры подключения берутся из файла настройки пункта #2) записываются дата-время и название скопированного файла.
+ После окончания копирования пользователю на экран выводится содержимое базы данных (скопированные файлы и дата-время их копирования).
____
Компиляция и запуск производилась на Ubuntu 20.04 LTS
```
g++ prog.cpp -o prog -lssh -lsqlite3
```
Требуются некторые библиотеки для С++:
```
sudo apt-get install libssh-dev
sudo apt-get install libsqlite3-dev
```

Файл настроек назван set.txt(можно любое).
Формат:
```
sftp_host=127.0.0.1
sftp_port=22
sftp_user=user
sftp_password=password
sftp_remote_dir=/path/to/remotedir
local_dir=/path/to/localdir
sql_user=user_name
sql_password=password
sql_database=database_name
```

Перед выполнением программы необходимо добавить удалённую систему в ssh known_hosts.

# Файлы:

+ prog.cpp - исходный код
+ prog - скомпилированная программа
+ create_db.sql - SQL скрипт для создания бд и таблицы в SQLite(можно не использовать, создается автоматически в программе)
+ set.txt - файл настроек (название или путь при запуске из другой папки указывается при запуске приложения) `./prog set.txt`
+ sftp.sqlite3 - БД используемая при тестировании(на последнем этапе, после добавления взаимодействия с SQLite3) 

