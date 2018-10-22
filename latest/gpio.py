#!/usr/bin/python3
# -*- coding: utf-8 -*-
'''
1. 重写的一个gpio控制程序，控制LED灯和两个电话接口
2. 固定死了gpio的方向，也可重置
3. 使用时直接调用IOCtral类即可
    属性：IOCtral.led
          IOCtral.tel
    方法：IOCtral.Reset()
          # 重置IO口为默认状态
          IOCtral.led.led(LED_serial_number, status)
          # 第一个参数为四个灯的序号1 2 3 4，第二个参数为状态
          IOCtral.led.flow(t)
          # 流水灯展示，缺省参数t代表时间，默认为5
          IOCtral.tel.listen(t=0)
          # 监听有无通话接入，若有，三秒后返回True，t为监听时长，若为0则永远监听
          IOCtral.tel.hook(status)
          # 摘机挂机，status为1代表接通，0为挂电话
4. __name__语句中是测试程序
'''
import RPi.GPIO as io
import time
import os 


class LED(object):

    def __init__(self):
        self.LED_channels = [7,8,9,10]
        io.setmode(io.BCM)

    def ResetLed(self):
        # reset the channel 7,8,9,10 to close all leds
        for channel in self.LED_channels:
            io.setup(channel, io.OUT)
            io.output(channel, 0)

    def led(self, number, status):
        # turn on or off a single led
        if status == 'on' or status == 1:
            io.output(self.LED_channels[number-1], 1)
        elif status == 'off' or status == 0:
            io.output(self.LED_channels[number-1], 0)
        else:
            print('LED:Invalid operation!')

    def flow(self, t=5):
        # flow lamp in time "t"
        self.ResetLed()
        start = time.time()
        while True:
            for i in range(4):
                self.led(i+1, 'on')
                time.sleep(0.1)
                self.led(i+1, 'off')
                time.sleep(0.1)
            if time.time() - start >= t:
                return 0


class TEL(object):

    def __init__(self):
        self.Ring_channel = 17
        self.Hook_channel = 2
        io.setmode(io.BCM)
    
    def ResetTel(self):
        io.setup(self.Ring_channel, io.IN, pull_up_down=io.PUD_UP)
        io.setup(self.Hook_channel, io.OUT)
        io.output(self.Hook_channel, 0)

    def listen(self, t=0):
        counter = 0
        start = time.time()
        while True:
            if io.input(self.Ring_channel) == io.LOW:
                counter += 1
                if counter == 3:
                    return True
            if t and (time.time() - start >= t):
                return False

    def hook(self, status):
        if status:
            io.output(self.Hook_channel, 1)
        else:
            io.output(self.Hook_channel, 0)


class IOCtrl(LED, TEL):

    def __init__(self):
        self.led = LED()
        self.tel = TEL()

    def Reset(self):
        self.led.ResetLed()
        self.tel.ResetTel()

if __name__ == '__main__':
    iopy = IOCtrl()
    iopy.Reset()
    print(u'Listening……')
    if iopy.tel.listen():
        print(u'There bell ring! Led run!')
        iopy.tel.hook(True)
        iopy.led.flow(t=5)
        for i in range(4):
        	os.system('./pcm_play') # 这里读取的for循环怎么 确定？
        iopy.led.led(1, 'on') # 亮灯后进入录音
        os.system('./capture > capture.wav')#录音
        os.system('./play < capture.wav')#播放
        iopy.tel.hook(False)
    # 挂机后播放录音
    '''
    播放录音
    '''
    # 程序结束

'''
if __name__ == '__main__':
    iopy = IOCtrl()
    iopy.Reset()
    print(u'Listening……')
    if iopy.tel.listen():
        print(u'There bell ring! Led run!')
        iopy.tel.hook(True)
        iopy.led.flow(t=5)
        for i in range(4):
        	os.system('./pcm_play')
        '''
        
        在这里播放一段10s的声音
        '''
        iopy.led.led(1, 'on') # 亮灯后进入录音
        '''
        在这里录音
        '''
        '''
        如果是对方先挂电话，如何检测到对面已挂机
        '''
        iopy.tel.hook(False)
    # 挂机后播放录音
    '''
    播放录音
    '''
    # 程序结束
'''
