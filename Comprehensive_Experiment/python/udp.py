#!/usr/bin/python

import base64
import hashlib
import hmac
import json
import threading
import urllib
import requests
import socket
import optparse
import time
import wave
import os
import asyncio
from datetime import datetime

# 导入API模块
import api

DEFAULT_IP   = '0.0.0.0'         # IP address of the UDP server
DEFAULT_PORT = 57345             # Port of the UDP server

# WAV文件参数
CHANNELS = 1                # 单声道
SAMPLE_WIDTH = 2           # 2字节 = 16位
FRAMERATE = 16000          # 采样率，常见值: 8000, 16000, 44100, 48000

# 结束标志位 - 当收到这个标志时表示数据传输结束
START_FLAG = b'aaaa'       # 对应客户端发送的音频开始标志
END_FLAG = b'flag'         # 对应客户端发送的音频结束标志
START_WITH_FLAG = b'C'     # 对应客户端发送的slider标志

# 特殊数字
START_NUM = 200
STOP_NUM = 300

def audio2text(file_path, language='cn', appid='d66588e7', secret_key='43ba882186bcd40e17b7257dce8cd3f3'):
    def get_signa(ts):
        m2 = hashlib.md5()
        m2.update((appid + ts).encode('utf-8'))
        md5 = m2.hexdigest()
        md5 = bytes(md5, encoding='utf-8')
        signa = hmac.new(secret_key.encode('utf-8'), md5, hashlib.sha1).digest()
        signa = base64.b64encode(signa)
        return str(signa, 'utf-8')

    def parse_result(transcript):
        """解析转写结果"""
        try:
            order_result_dict = json.loads(transcript)
            concatenated_text = ''
            for lattice_entry in order_result_dict['lattice']:
                json_1best_value = json.loads(lattice_entry['json_1best'])
                ws_list = json_1best_value['st']['rt'][0]['ws']
                for word in ws_list:
                    concatenated_text += word['cw'][0]['w']
            return concatenated_text
        except Exception as e:
            print(f"解析错误: {e}")
            return None

    lfasr_host = 'https://raasr.xfyun.cn/v2/api'
    api_upload = '/upload'
    api_get_result = '/getResult'

    try:
        # 1. 上传文件
        ts = str(int(time.time()))
        signa = get_signa(ts)
        file_len = os.path.getsize(file_path)
        file_name = os.path.basename(file_path)

        param_dict = {
            'appId': appid,
            'signa': signa,
            'ts': ts,
            'fileSize': file_len,
            'fileName': file_name,
            'duration': '200',
            'language': language
        }

        with open(file_path, 'rb') as f:
            data = f.read(file_len)

        upload_response = requests.post(
            url=lfasr_host + api_upload + "?" + urllib.parse.urlencode(param_dict),
            headers={"Content-type": "application/json"},
            data=data
        )
        upload_result = json.loads(upload_response.text)

        if upload_result['code'] != '000000':
            print(f"上传失败: {upload_result.get('descInfo', '未知错误')}")
            return None

        # 2. 获取转写结果
        orderId = upload_result['content']['orderId']
        param_dict = {
            'appId': appid,
            'signa': signa,
            'ts': ts,
            'orderId': orderId,
            'resultType': 'transfer,predict'
        }

        status = 3
        while status == 3:
            time.sleep(3)
            response = requests.post(
                url=lfasr_host + api_get_result + "?" + urllib.parse.urlencode(param_dict),
                headers={"Content-type": "application/json"}
            )
            result = json.loads(response.text)
            status = result['content']['orderInfo']['status']
            if status == 4:
                break

        # 3. 处理最终结果
        if result['code'] == '000000':
            transcript = result['content']['orderResult']
            return parse_result(transcript)
        else:
            print(f"转写失败: {result.get('descInfo', '未知错误')}")
            return "None"

    except Exception as e:
        print(f"转写异常: {str(e)}")
        return "None"

def save_as_wav(buffer, filename, channels=CHANNELS, sample_width=SAMPLE_WIDTH, framerate=FRAMERATE):
    """将缓冲区数据保存为WAV文件"""
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(channels)           # 设置通道数
        wav_file.setsampwidth(sample_width)       # 设置采样宽度
        wav_file.setframerate(framerate)          # 设置帧率/采样率
        wav_file.writeframes(buffer)              # 写入音频数据
    return os.path.abspath(filename)


async def echo_server(host, port):
    print("============================================================")
    print("UDP音频接收服务器")
    print("============================================================")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setblocking(False)
    sock.bind((host, port))
    print('UDP服务器IP地址: {} 端口 {}'.format(host, port))
    print('等待接收来自UDP客户端的消息...')

    # 初始化变量
    buffer = bytearray()           # 用于保存所有接收到的音频数据
    client_addr = None             # 客户端地址
    packets_received = 0           # 收到的数据包数量
    recording = False              # 是否正在录制音频
    last_packet_time = None        # 最后一个数据包的时间
    audio_started = False          # 是否已收到音频开始标志
    # txt_filename = "control_data.txt"  # 控制数据保存的文件名

    # 新增控制数据监测相关变量
    control_history = []           # 存储控制数据历史记录
    monitoring = True              # 是否正在监测趋势
    THRESHOLD = 70                 # 变化阈值

    loop = asyncio.get_running_loop()

    try:
        while True:
            try:
                data, addr = await loop.sock_recvfrom(sock, 225000)

                # 检查是否是控制数据 (以b'C'开头)
                if data.startswith(b'C'):
                    try:
                        # 提取数字部分
                        control_num = int(data[1:].decode('ascii'))
                        print(f"收到控制数据: {control_num}")

                        # 控制数据趋势分析
                        if monitoring:
                            control_history.append(control_num)

                            # 当有足够数据时开始分析
                            if len(control_history) >= 2:
                                # 检查当前趋势方向
                                current_trend = None
                                if control_history[-1] > control_history[-2]:
                                    current_trend = "rising"
                                elif control_history[-1] < control_history[-2]:
                                    current_trend = "falling"

                                # 如果趋势方向改变，重置历史记录
                                if len(control_history) >= 3:
                                    prev_trend = None
                                    if control_history[-2] > control_history[-3]:
                                        prev_trend = "rising"
                                    elif control_history[-2] < control_history[-3]:
                                        prev_trend = "falling"

                                    if current_trend != prev_trend and prev_trend is not None:
                                        control_history = control_history[-2:]  # 保留最后两个值
                                        continue

                                # 检查是否达到阈值
                                if len(control_history) >= 2:
                                    diff = abs(control_history[-1] - control_history[0])
                                    if diff >= THRESHOLD:
                                        if current_trend == "rising":
                                            print("True (上升趋势)")
                                            # 调用API返回给前端number:3
                                            asyncio.create_task(api.send_trend_rising())
                                        elif current_trend == "falling":
                                            print("False (下降趋势)")
                                            # 调用API返回给前端number:2
                                            asyncio.create_task(api.send_trend_falling())
                                        # 重置监测状态
                                        control_history = []


                        # 特殊控制命令处理
                        if control_num == START_NUM:
                            print("收到开始命令")
                            control_history = []
                            monitoring = True
                            # 调用API返回给前端number:1
                            asyncio.create_task(api.send_control_start())
                        elif control_num == STOP_NUM:
                            print("收到停止命令")
                            control_history = []
                            monitoring = True
                            # 调用API返回给前端number:0
                            asyncio.create_task(api.send_control_stop())

                    except (ValueError, UnicodeDecodeError):
                        print(f"收到无效控制数据: {data}")
                        # 调用API返回给前端number:-1
                        asyncio.create_task(api.send_control_invalid())
                        continue

                # 检查是否是音频开始标志
                if data == START_FLAG:
                    # 如果是新客户端或之前有录制，先保存之前的音频
                    if recording and len(buffer) > 0:
                        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                        filename = f"audio_{timestamp}.wav"
                        filepath = save_as_wav(buffer, filename)
                        print(f"\n音频文件已保存: {filepath}")
                        print(f"接收到 {packets_received} 个数据包，总大小: {len(buffer)} 字节")

                    # 重置录制状态
                    buffer = bytearray()
                    recording = True
                    audio_started = True
                    client_addr = addr
                    packets_received = 0
                    print(f"\n开始从 {addr[0]}:{addr[1]} 接收新的音频流")
                    continue

                # 检查是否是音频结束标志
                if data == END_FLAG:
                    if recording and len(buffer) > 0:
                        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                        filename = f"audio_{timestamp}.wav"
                        filepath = save_as_wav(buffer, filename)
                        print(f"\n收到结束标志，音频文件已保存: {filepath}")
                        print(f"接收到 {packets_received} 个数据包，总大小: {len(buffer)} 字节")

                        text = await loop.run_in_executor(None, audio2text, filepath)
                        print(f"语音识别返回：{text}")
                        asyncio.create_task(api.send_audio_text(text))

                    # 重置状态
                    buffer = bytearray()
                    packets_received = 0
                    audio_started = False
                    recording = False
                    print(f"缓冲区已重置，等待新的音频流")
                    continue

                # 如果是音频数据且已经开始录制
                elif recording and audio_started:
                    buffer.extend(data)
                    packets_received += 1

            except BlockingIOError:
                await asyncio.sleep(0.001)  # 短暂休眠，释放控制权
            except ConnectionResetError:
                print("客户端已断开连接。")
                break
            except Exception as e:
                print(f"发生错误: {e}")
                break

    except KeyboardInterrupt:
        print("\n用户中断, 正在退出...")
        # 保存最后的录音
        if len(buffer) > 0:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"audio_{timestamp}.wav"
            filepath = save_as_wav(buffer, filename)
            print(f"最终音频文件已保存: {filepath}")
            print(f"共接收 {packets_received} 个数据包，总大小: {len(buffer)} 字节")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        sock.close()
        print("UDP服务器已关闭")