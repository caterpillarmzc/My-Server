#include"db.hpp"

void TestImageTable(){
  //用Json转化成字符串,使用StyledWriter的目的是让转化出的字符串具有一定的格式,便于查看
  Json::StyledWriter writer;
  //创建一个ImageTable类,调用其中的方法,验证结
  MYSQL* mysql = image_system::MySQLInit();
  image_system::ImageTable image_table(mysql);
  bool ret=false;
  //插入数据
 // Json::Value image;
 // image["image_name"]="test.png";
 // image["size"]=1024; 
 // image["upload_time"]="2019/09/01"; 
 // image["md5"]="123456"; 
 // image["type"]="png"; 
 // image["path"]="data/test.png";
 // ret=image_table.Insert(image);
 // printf("ret=%d\n",ret);
 
 //查找所有图片信息
 //Json::Value images;
 //ret=image_table.SelectALL(&images);
 //  printf("ret=%d\n",ret);
  // printf("%s\n",writer.write(images).c_str());//转化成c风格的字符串
  
  //查找指定图片信息
 // Json::Value image;
  //ret=image_table.SelectOne(1,&image);
  //printf("ret=%d\n",ret);
  //printf("%s\n",writer.write(image).c_str());

  //删除指定图片
  ret=image_table.Delete(1);
  printf("ret=%d\n",ret);
  image_system::MySQLRelease(mysql);
}

int main(){
  TestImageTable();
  return 0;
}
