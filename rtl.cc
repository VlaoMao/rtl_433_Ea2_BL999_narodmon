#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

//
#include "boost/ptree.hpp"
#include "boost/json_parser.hpp"
//

#include "sockets.h"
#include "serial/SerialPort.hpp"

using namespace std::chrono_literals;

typedef std::pair<std::string, std::string> pair;
typedef std::vector<std::string> string_vector;


std::mutex lock_mutex;
string_vector str_vec;
std::atomic<bool> thread_alive(true);
std::string device_mac;
std::string device_id;
std::string prev_humidity;

typedef std::map<const std::string, std::string> sensor_values_map;

const std::string &temp_key = "temperature_C";
const std::string &humidity_key = "humidity";
const std::string &id_key = "id";

sensor_values_map etalon_values = {
    {"time", ""},
    {"model", ""},
    {id_key, ""},
    {"channel", ""},
    {"battery", ""},
    {temp_key, ""},
    {humidity_key, ""}
};

void print_container(string_vector &vec)
{
    std::cout << "Container: \n";
    for(const auto& str: vec)
        std::cout << str << std::endl;
}

void insertString(string_vector &vec, std::string string)
{
    std::lock_guard<std::mutex> lock(lock_mutex);

    vec.emplace_back(string);
}

void clearStringContainer(string_vector &vec)
{
    std::lock_guard<std::mutex> lock(lock_mutex);

    vec.clear();
}

void t_read_values(const std::string& usb_dev)
{
    mn::CppLinuxSerial::SerialPort serialPort;
    std::chrono::duration<double> timeout_interval(1);
    
    try {
        serialPort.SetDevice(usb_dev);
        serialPort.SetBaudRate(mn::CppLinuxSerial::BaudRate::B_115200);
        serialPort.SetTimeout(0);
        serialPort.Open();

        std::string data_acc;
        std::string data;
        auto time_read_start = std::chrono::high_resolution_clock::now();
        auto time_read_elapsed = time_read_start;

        while(true) {
            serialPort.Read(data_acc);
            if(data_acc.empty()) {
                if((time_read_elapsed - time_read_start) > timeout_interval) {
                    //timeout read
                    if(!data.empty()) {
                        std::cout << "readed data: " << data << std::endl;
                        insertString(str_vec, data);
                        data.clear();
                    }
                }
                time_read_elapsed = std::chrono::high_resolution_clock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                time_read_start = std::chrono::high_resolution_clock::now();
                time_read_elapsed = time_read_start;
                data += data_acc;
            }
        }

    } catch (std::exception& e) {
        std::cerr << "Error reading values: " << e.what() << "\n";
        thread_alive = false;
    }
}

sensor_values_map parse_json(const std::string &str)
{
    sensor_values_map ret;

    boost::property_tree::ptree pt;

    try {
        std::stringstream ss;
        ss << str;
        boost::property_tree::read_json(ss, pt);

        std::string temp;
        std::string humidity;
        for (auto& array_element : pt) {
            if(etalon_values.find(array_element.first) == etalon_values.end()) // Если не нашлось элемента с таким ключом, идём дальше
                continue ;
            else
                ret[array_element.first] = array_element.second.get_value<std::string>();
            /*
            if(array_element.first == "temperature_C")
                temp = array_element.second.get_value<std::string>();

            if(array_element.first == "humidity")
                humidity = array_element.second.get_value<std::string>();
            */
        }

        //if((!temp.empty()) && (!humidity.empty()))
        //    ret = pair(temp, humidity);
    } catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;

    }

    return ret;
}

void uploadToNarodmon(std::string temp, std::string humidity)
{
    std::string device_name = "rtl_433";
    std::string lat = "53.201280";
    std::string lng = "44.994961";
    std::string sensor_desc_1 = "Температура на улице";
    std::string sensor_desc_2 = "Влажность";

    if(temp.empty() || humidity.empty() || device_mac.empty())
        return ;

    std::stringstream str_desc;

    str_desc << "#" << device_mac << "#" << device_name << "\n";
    str_desc << "#T1#" << temp << "#" << sensor_desc_1 << "\n";
    str_desc << "#H1#" << humidity << "#" << sensor_desc_2 << "\n";

    str_desc << "#LAT#" << lat << "\n";
    str_desc << "#LNG#" << lng << "\n";
    str_desc << "##\n";

    std::cout << "str to send: \n" << str_desc.str() << std::endl;

    int fd = create_connect_socket("narodmon.ru", "8283");
    if(fd < 0) {
        std::cerr << "Failed to connect to narodmon\n";
        return ;
    }

    int result = send_to_socket(fd, str_desc.str().c_str(), strlen(str_desc.str().c_str()));
    if(result <= 0) {
        std::cerr << "Error sending data\n";
        close_socket(fd);
        return ;
    }

    char read_buf[1024] = {0,};
    if(read_from_socket(fd, read_buf, 1024) <= 0) {
        std::cerr << "Error reading answer\n";
        close_socket(fd);
        return ;
    }

    std::cout << "answer: " << read_buf << std::endl;

    close_socket(fd);
}

void run_update()
{
    std::string temp;
    std::string humidity;
    std::string id;

    std::cout << "run update\n";
    auto rit = str_vec.rbegin();
    for(; rit != str_vec.rend(); rit ++) {
        sensor_values_map ret = parse_json(*rit);
        if((ret[temp_key].empty()) || (ret[humidity_key].empty()) || (ret[id_key].empty()))
            continue;

        temp = ret[temp_key];
        humidity = ret[humidity_key];
        id = ret[id_key];

        if((humidity == "99") && (prev_humidity == "99")) {
                humidity = "98";
                prev_humidity = "98";
        } else {
            prev_humidity = humidity;
        }

        //std::cout << "Found temp: " << temp << ", humidity: " << humidity << ", id: " << id << " id == device_id: " << (id == device_id) << std::endl;
        if(id != device_id)
            continue ;

        uploadToNarodmon(temp, humidity);
        break;
    }

    clearStringContainer(str_vec);
}

int main(int argc, char **argv)
{
    if(argc < 4) {
        std::cerr << "Usage: " << argv[0] << "/dev/ttyUSBX device_mac device_id\n";
        return 1;
    }


    std::string usb_dev = argv[1];
    device_mac = argv[2];
    device_id = argv[3];

    //insertString(str_vec, "{\"time\" : \"2019-04-11 21:57:47\", \"model\" : \"TFA-Twin-Plus-30.3049\", \"id\" : 46, \"channel\" : 1, \"battery\" : \"OK\", \"temperature_C\" : 12.900, \"humidity\" : 48, \"mic\" : \"CHECKSUM\"}");
    //insertString(str_vec, "{\"id\" : 37, \"channel\" : 2, \"battery\" : \"OK\", \"temperature_C\" : 23.40, \"humidity\" : 20}");

    std::thread in_thread(t_read_values, usb_dev);
    auto time_start = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> refresh_interval(6 * 60); // 6 min
    //std::chrono::duration<double> refresh_interval(5); // 5 sec

    while(thread_alive) {
        auto time_current = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = time_current - time_start;
        if(elapsed >= refresh_interval) {
            time_start = std::chrono::high_resolution_clock::now();
            run_update();
        }

        std::this_thread::sleep_for(1s);
    }

    in_thread.join();
    return 1;
}
