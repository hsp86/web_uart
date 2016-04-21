#-*- coding: utf-8 -*-
import sys,os
import web
import serial

import config

# reload(sys)#支持中文
# sys.setdefaultencoding('utf8')

urls = (
    '/','index',
    '/execute','execute'
    )

render = web.template.render(config.temp_dir)

class index:
    def GET(self):
        return render.index()


class execute:
    def GET(self):
        get_data = web.input(cmd={})
        cmd = get_data.cmd # 命令字符
        ser = serial.Serial(config.com_port,9600)
        ser.write(cmd)
        num = 0
        c = ser.read(1)
        while '\n' != c: #每次读取一个判断为换行才算读完
            num = num * 256 + ord(c) # 将字符转换为对应ascii码；其它转化：int,float,str,chr
            c = ser.read(1)
        ser.close()
        return str(num)

def nf():
    return web.notfound("胡祀鹏 提示：Sorry, the page you were looking for was not found.")

def ine():
    return web.internalerror("胡祀鹏 提示：Bad, bad server. No donut for you.")

if __name__ == '__main__':
    webpy_app = web.application(urls,globals())
    webpy_app.notfound = nf#自定义未找到页面
    webpy_app.internalerror = ine#自定义 500 错误消息
    webpy_app.run()
    print 'webpy bay!'


