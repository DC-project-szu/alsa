# 阅读代码笔记

## sound.c

### 线程函数框架
1. 各种初始化
2. 进入循环
    + `frames = sound_read(rhandle, InBuf, SPEEX_SAMPLES);`，从声卡读数据
    + `sound_split(InBuf, InputBuf[RADIO], InputBuf[PHONE] , SPEEX_SAMPLES * 2);`，分声道
    + `if (toneDetecting((short*)InputBuf[PHONE]) > 7){`，检测忙音
    + `if (time_status)`，检测超时
    + 进入switch-case，下文中的七个状态
    + `sound_combine(OutBuf, OutputBuf[RADIO], OutputBuf[PHONE] , SPEEX_SAMPLES * 2);`，合并声道
    + `frames = sound_write(whandle, OutBuf, SPEEX_SAMPLES);`,写数据到声卡
+ 如果循环结束就处理后事

### 各种状态：
#### 0.LINE_IDLE 空闲状态
1. 如果这时有电话打来`UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER`, 将进入以下状态之一:
    1. LINE_INCOMING 如果设置了需要密码
    2. LINE_TALKING 没设置密码就直接可以talk
2. `UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER`
    1. `RADIO_RX()`为1
        + 读取、检查密码，成功后就接电话，开启PTT，进入`LINE_OUTGOING`
3. `UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY`
    + 这个貌似是网络相关的
4. `UserConfig.TelephoneCfg.Mode == TELEPHONE_TRBO`
    + 没实现
5. `UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER`
    1. `RADIO_RX()`为1
        + 读取、检查密码，成功就接电话，开启PTT，进入`LINE_TALKING`
6. `UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR`
    1. `sound_speech(InputBuf[RADIO], 0)`为1，说明为speech模式
        + 读取、检查密码，成功后就接电话，开启PTT，进入`LINE_OUTGOING`

####  1.LINE_INCOMING
1. 读取密码
2. `password != NULL`
    1. 检查成功之后，如果`UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER`为真就关闭PTT，
最后进入`LINE_TALKING`
    2. `retry++ > 2`
        + 播放结束音频，进入`LINE_ENDING`
    3. `status = LINE_RETRY`,然后播放重试音频
    4. `sound_file_stopping`
        + `memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);`
        + `continue`
    5. `sound_file_playing`
        + ...


#### 2.LINE_OUTGOING
1. `(UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO) ||
    (UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR)`
    1. `time_after(time(NULL), audio_start + 3)`，即播放手机声音三秒了
        + 关闭PTT，进入`LINE_DAILING`
    2. `memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );`，从手机input拷贝数据到车台output
2. `UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY`
    1. `time_after(time(NULL), audio_start + 3)`
        + 进入`LINE_DAILING`
    2. `send_audio(InputBuf[PHONE], 1024, 0, 0);`，可能是这个网络状态下直接传数据


#### 3.LINE_RETRY
和`LINE_INCOMING`一样

#### 4.LINE_ENDING
播放结束音频给手机，然后挂电话

#### 5.LINE_TALKING
1. `UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO`
    1. `RADIO_RX()`为1
        1. `audio_telephone`
            + `DISABLE_PTT();`，如果车台在讲话，关闭手机
        2. 读取密码，密码为`(*password == '*') && (*(password+1) == '#')`就挂掉
        3. `memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );`，从车台input拷贝数据到手机output
        4. `memset( OutputBuf[RADIO], 0, SPEEX_SAMPLES * 2);`，清空车台output的数据
    2. `RADIO_RX()`为0
        详见代码
2. `UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY`
同前
3. `UserConfig.TelephoneCfg.Mode == TELEPHONE_TRBO`
没实现
4. `UserConfig.TelephoneCfg.Mode == TELEPHONE_REPEATER`
    + 读取、检查密码
    + `memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );`，从车台input拷贝数据到手机output
    + `memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );`，从手机input拷贝数据到车台output
5. `UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR`
    1. `sound_speech(InputBuf[RADIO], 0)`
        + 如果在说话就关掉手机(`DISABLE_PTT()`)
        + 读取密码，密码为`(*password == '*') && (*(password+1) == '#')`就挂掉
        + `memcpy( OutputBuf[RADIO], InputBuf[PHONE], SPEEX_SAMPLES * 2 );`，从手机input拷贝数据到车台output
        + `memset( OutputBuf[PHONE], 0, SPEEX_SAMPLES * 2);`，清空手机output的数据


#### 6.LINE_DAILING
1. `UserConfig.TelephoneCfg.Mode == TELEPHONE_RADIO`
    1. `RADIO_RX()`
        + 进入`LINE_TALKING`
    2. `memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );`，从车台input拷贝数据到手机output
2. `UserConfig.TelephoneCfg.Mode == TELEPHONE_GATEWAY`
    1. `GetAudioFrame(&AudioFrame)`,(获取车台输入的数据)
        + `memcpy( OutputBuf[PHONE], &SPXBuffer, sizeof(SPXBuffer));`，从车台input拷贝数据到手机output
    2. `time_after(time(NULL), audio_start + 3)`
        + 进入`LINE_TALKING`
3. `UserConfig.TelephoneCfg.Mode == TELEPHONE_NOCOR`
    1. `sound_speech(InputBuf[RADIO], 0)`
        + 进入`LINE_TALKING`
    2. `memcpy( OutputBuf[PHONE], InputBuf[RADIO], SPEEX_SAMPLES * 2 );`，从车台input拷贝数据到手机output
