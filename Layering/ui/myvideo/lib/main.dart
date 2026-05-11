import 'package:flutter/material.dart';
import 'videoui/main_screen.dart'; // 导入主界面

void main() {
  // 运行应用，显示主界面
  runApp(const MaterialApp(
    debugShowCheckedModeBanner: false,
    home: MainScreen(),
  ));
}