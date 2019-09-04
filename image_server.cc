#include<fstream>
#include"httplib.h"
#include<time.h>
#include<openssl/md5.h>
#include"db.hpp"
#include<signal.h>
#include <sys/stat.h>

class FileUtil {
	public:
		static bool Write(const std::string& file_name,
				const std::string& content) {
			std::ofstream file(file_name.c_str());
			if (!file.is_open()) {
				return false;

			}
			file.write(content.c_str(), content.length());
			file.close();
			return true;
		}
		static bool Read(const std::string& file_name,std::string* content){
			std::ifstream file(file_name.c_str());
			if (!file.is_open()){
				return false;
			}
			//要读取文件需要先知道文件的大小,把string* content当成是一个缓冲区
			//用stat获取文件大小,需要先创建一个stat struct
			struct stat st;
			stat(file_name.c_str(), &st);//读指定目录的文件转成字符串
			content->resize(st.st_size);//把字符串content的长度设置成跟文件一样
			//把文件一次性都读取完
			//file.read可以按照指定长度来读取
			//char*为缓冲区大小,int为读取的长度
			file.read((char*)content->c_str(), content->size());
			file.close();
			return true;
		}
};

MYSQL* mysql=NULL;
int main(){
	using namespace httplib;
	//在程序刚运行时就连接数据库
	mysql=image_system::MySQLInit();
	image_system::ImageTable image_table(mysql);
	//注意数据库需要关闭,关闭的时机就是服务端主动关闭时
	//服务端关闭是通过ctrl+c,所以可以通过捕捉2号信号来确定关闭数据库的时机
	signal(SIGINT,[](int){
			image_system::MySQLRelease(mysql);
			exit(0);
			});



	Server server;
	//表示当客户端求/hello路径(/path)时,执行一个特定的函数
	//指定不同的路径对应到不同的函数上,这称为路由
	//使用的是lambda表达式
	//Request:请求,可读不可写
	//Response:响应,可读可写
	//[&image_table]这是lambda的重要特性,捕获变量
	//lambda内部是不能直接访问image_table的,但是通过捕获就可以了,&相当于引用
	server.Post("/image",[&image_table](const Request& req,Response& resp){
			Json::Value resp_json;
			Json::FastWriter writer;
			printf("上传图片\n");
			//1.对参数进行校验
			//auto size = req.files.size();//size为图片的个数
			auto ret = req.has_file("upload");
			if(!ret){
			printf("上传文件出错!!!\n");
			resp.status=404;
			//可以使用json格式组织一个返回结果
			resp_json["ok"]=false;
			resp_json["reason"]="上传文件出错,没有需要的upload字段";
			resp.set_content(writer.write(resp_json),"application/json");
			return;
			}
			//2.根据文件名获取到文件的数据file对象
			const auto& file = req.get_file_value("name1");
			// file.filename;
			// file.content_type;
			//上传图片的内容

			//3.把图片的属性信息插入到数据库中
			Json::Value image;
			image["image_name"] = file.filename;
			image["size"] = (int)file.length;
			time_t tt;
			time(&tt);
			tt = tt + (8*3600);
			tm* t = gmtime(&tt);
			char res[1024] = {0};
			sprintf(res,"%d-%02d-%02d %02d:%02d:%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
			image["upload_time"] = res;
			std::string md5value;
			auto body = req.body.substr(file.offset, file.length);
			md5(body,md5value);
			image["md5"] = md5value;
			image["type"] = file.content_type;
			image["path"] = "./data/" + file.filename;
			ret = image_table.Insert(image);
			if (!ret) {
				printf("image_table Insert failed!\n");
				resp_json["ok"] = false;
				resp_json["reason"] = "数据库插入失败!";
				resp.status = 500;
				resp.set_content(writer.write(resp_json), "application/json");
				return;
			}
			//4.把图片保存到指定的磁盘目录
			auto body = req.body.substr(file.offset, file.length);
			FileUtil::Write(image["path"].asString(), body);
			//5.构造一个响应数据通知客户端上传成功
			resp_json["ok"] = true;
			resp.status = 200;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
	});


	server.Get("/image",[&image_table](const Request& req,Response& resp){
			printf("获取所有图片信息\n");
			Json::Value resp_json;
			Json::FastWriter writer;

			//调用数据库接口来获取数据
			bool ret=image_table.SelectALL(&resp_json);
			if(!ret){
			printf("查询数据库失败!\n");
			resp_json["ok"] = false;
			resp_json["reason"] = "查询数据库失败!";
			resp.status = 500;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			}
			//2.构造响应结果返回给客户端
			resp.status = 200;
			resp.set_content(writer.write(resp_json), "application/json");
			});


	server.Get(R"(/image/(\d+))",[&image_table](const Request& req,Response& resp){
			Json::FastWriter writer;
			Json::Value resp_json;
			//1.先获取到图片 id
			int image_id = std::stoi(req.matches[1]);
			printf("获取 id 为 %d 的图片信息!\n", image_id);
			// 2. 再根据图片 id 查询数据库
			bool ret = image_table.SelectOne(image_id, &resp_json);
			if (!ret) {
			printf("数据库查询出错!\n");
			resp_json["ok"] = false;
			resp_json["reason"] = "数据库查询出错";
			resp.status = 404;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			}
			// 3. 把查询结果返回给客户端
			resp_json["ok"] = true;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			});


	server.Get(R"(/show/(\d+))",[&image_table](const Request& req,Response& resp){
			Json::FastWriter writer;
			Json::Value resp_json;
			Json::Value image;
			// 1. 根据图片 id 去数据库中查到对应的目录
			int image_id = std::stoi(req.matches[1]);
			printf("获取 id 为 %d 的图片内容!\n", image_id);
			bool ret = image_table.SelectOne(image_id, &image);
			if(!ret){
			printf("读取数据库失败!\n");
			resp_json["ok"] = false;
			resp_json["reason"] = "数据库查询出错";
			resp.status = 404;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			}
			// 2. 根据目录找到文件内容, 读取文件内容
			std::string image_body;
			printf("%s\n", image["path"].asCString());
			ret = FileUtil::Read(image["path"].asString(), &image_body);
			if (!ret) {
				printf("读取图片文件失败!\n");
				resp_json["ok"] = false;
				resp_json["reason"] = "读取图片文件失败";
				resp.status = 500;
				resp.set_content(writer.write(resp_json), "application/json");
				return;
			}
			//3. 把文件内容构造成一个响应
			resp.status = 200;//状态码
			//正文应该是图片信息,图片的类型与数据库中的图片type一致
			resp.set_content(image_body, image["type"].asCString());
	});


	server.Delete(R"(/image/(\d+))",[&image_table](const Request& req,Response& resp){
			// 1. 根据图片 id 去数据库中查到对应的目录
			int image_id = std::stoi(req.matches[1]);
			printf("删除 id 为 %d 的图片!\n", image_id);
			// 2. 查找到对应文件的路径
			Json::Value image;
			Json::FastWriter writer;
			Json::Value resp_json;
			bool ret = image_table.SelectOne(image_id, &image);
			if (!ret) {
			printf("查找要删除的图片文件失败!\n");
			resp_json["ok"] = false;
			resp_json["reason"] = "删除图片文件失败";
			resp.status = 404;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			}
			// 3. 调用数据库操作进行删除
			ret = image_table.Delete(image_id);
			if (!ret) {
			printf("删除图片文件失败!\n");
			resp_json["ok"] = false;
			resp_json["reason"] = "删除图片文件失败";
			resp.status = 404;
			resp.set_content(writer.write(resp_json), "application/json");
			return;
			}
			// 4. 删除磁盘上的文件
			// C++ 标准库中不能删除文件,只能使用操作系统提供的函数
			unlink(image["path"].asCString());
			// 5. 构造响应
			resp_json["ok"] = true;
			resp.status = 200;
			resp.set_content(writer.write(resp_json), "application/json");
	});
	//设置一个静态资源的目录
	server.set_base_dir("./wwwroot");
	server.listen("0.0.0.0",9094);
	return 0;
}
