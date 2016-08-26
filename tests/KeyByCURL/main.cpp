#include <string>
#include <sstream>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <iomanip>

const std::string URL = "http://localhost4/qchannel/1/1";

int main (int argc, char** argv)
{
    using namespace std;

    //Создадим ключ с псевдослучайной последовательностью бит
    srand(time(NULL));
    //Размер ключа
    size_t size = rand()%2048;//Для определённости положим, что это будет наибольший размер ключа 
    vector<bool> key(size);
    for (int i = 0; i < key.size(); i++) key[i] = rand()%2;

    //Сформируем команду для отправки curl-запроса потребителю.
    //Ключ передаётся в шестнадцатеричном виде в видe ascii-текста
    //Если чисто бит не кратно четырём, то ненужные биты с конца отбрасываем -
    //невелика потеря
    {
        string cmd = "curl --data-ascii \"";
        stringstream ss;
        //Отбросим некратные четырём биты с конца
        key.resize((key.size()/4)*4);

        //Добавим число байт
        ss << key.size() << '\n';

        //Теперь будем формировать по одному hex-символу и добавлять ко строке
        for (size_t i = 0; i < key.size()/4; i++)
        {
            int val = 0;
            for (size_t j = 0; j < 4; j++) val |= key[i*4 + j] << j;
            ss << setbase(16) << val;
        }
        ss << "\" http://";

        //Если URL назначения не указан, то по умолчанию это localhost4
        if (argc == 1) ss << "localhost4";
        else ss << argv[1];

        //Добавим статус канала
        ss << "/qchannel/1/1";

        cmd += ss.str();
        system(cmd.c_str());
        //cout << cmd << endl;
    }
}