#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <string>
#include <ctime>
#include <iomanip>

using namespace std;

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

bool starts_with(string message, string n){
    if(message.length() < n.length())
        return false;
    for(int x; x < n.length();x++){
        if(message[x] != n[x])
            return false;
    }
    return true;
}


std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            if(starts_with(message,"LOG")){
                std::vector<std::string> message_div;
                boost::split(message_div, message, boost::is_any_of("|"));
                LogRecord sensor;
                strcpy(sensor.sensor_id,message_div[1].c_str());
                sensor.timestamp = string_to_time_t(message_div[2]);
                sensor.value = stod(message_div[3]);
                std::fstream file("log.dat", std::fstream::out | std::fstream::in | std::fstream::binary 
                                                                     | std::fstream::app); 
                if (file.is_open())
                {
                    file.write((char*)&sensor, sizeof(LogRecord));
                    file.close();
                }
                else
                {
                    std::cout << "Error opening file!" << std::endl;
                }
            }

            if(starts_with(message,"GET")){
                std::vector<std::string> message_div;
                boost::split(message_div, message, boost::is_any_of("|"));
                int tam_reg = std::stoi(message_div[2]);
                std::fstream file("log.dat", std::fstream::out | std::fstream::in | std::fstream::binary 
                                                                     | std::fstream::app); 
                if (file.is_open())
                {
                          // Obtenha o tamanho do arquivo em bytes
                  file.seekg(0, std::fstream::end);
                  int file_size = file.tellg();
                  int num_records = file_size / sizeof(LogRecord);

                  if (num_records > 0)
                  {
                      // Calcule o índice do primeiro registro a ser lido
                      int start_index = num_records - tam_reg;
                      if (start_index < 0)
                      {
                          start_index = 0;
                          tam_reg = num_records;
                      }

                      // Posicione o ponteiro de leitura no início do primeiro registro a ser lido
                      file.seekg(start_index * sizeof(LogRecord), std::fstream::beg);

                      // Crie um vetor temporário para armazenar os registros
                      std::vector<LogRecord> records(tam_reg);

                      // Leia os registros do arquivo
                      file.read(reinterpret_cast<char*>(records.data()), tam_reg * sizeof(LogRecord));
                      string date;
                      // Imprima os registros
                      for (const auto& record : records)
                      {
                        date = time_t_to_string(record.timestamp);
                          // Imprima o conteúdo do registro conforme necessário
                        std::cout << message_div[2] << ";" << date <<"|"<< record.value<<"\n";

                      }
                  }
                  else
                  {
                      std::cout << "No records found in the file." << std::endl;
                  }

                  file.close();
                }
                else
                {
                    std::cout << "Error opening file!" << std::endl;
                }
                

            }
            std::cout << "Received: " << message << std::endl;
            write_message(message);
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}
