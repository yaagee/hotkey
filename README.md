# hotkey
注册热键，可快速运行任务，可启动，隐藏，杀死进程。

配置文件hotkeys.json例子：
{
	"hotkeytasks": {
	"Z":"E:\programfiles\notepad.EXE",
	},
   "hotkeyhidetasks": {
	   "H":"notepad.exe",
   },
   "hotkeykilltasks": {
	   "X":"notepad.exe",
   }
}

这样按ctrl + shift + z 会执行：E:\programfiles\notepad.EXE
ctrl + shift + h 会隐藏notepad进程
ctrl + shift + x 会杀掉notepad进程

有些热键会注册失败，这样会导致程序功能失效

编译：
vs2017 + qt5.14.2-x64

注意事项： 要操控高权限的进程，确保hotkey程序用管理员方式运行
