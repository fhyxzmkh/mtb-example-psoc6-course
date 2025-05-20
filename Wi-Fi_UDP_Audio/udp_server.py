#******************************************************************************
# File Name:   udp_server.py
#
# Description: A simple "udp server" for demonstrating UDP usage.
# The server sends LED ON/OFF commands to the connected UDP client
# and receives acknowledgement from the client.
#
#********************************************************************************
# Copyright 2020-2024, Cypress Semiconductor Corporation (an Infineon company) or
# an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
#
# This software, including source code, documentation and related
# materials ("Software") is owned by Cypress Semiconductor Corporation
# or one of its affiliates ("Cypress") and is protected by and subject to
# worldwide patent protection (United States and foreign),
# United States copyright laws and international treaty provisions.
# Therefore, you may use this Software only as provided in the license
# agreement accompanying the software package from which you
# obtained this Software ("EULA").
# If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
# non-transferable license to copy, modify, and compile the Software
# source code solely for use in connection with Cypress's
# integrated circuit products.  Any reproduction, modification, translation,
# compilation, or representation of this Software except as specified
# above is prohibited without the express written permission of Cypress.
#
# Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
# reserves the right to make changes to the Software without notice. Cypress
# does not assume any liability arising out of the application or use of the
# Software or any product or circuit described in the Software. Cypress does
# not authorize its products for use in any products where a malfunction or
# failure of the Cypress product may reasonably be expected to result in
# significant property damage, injury or death ("High Risk Product"). By
# including Cypress's product in a High Risk Product, the manufacturer
# of such system or application assumes all risk of such use and in doing
# so agrees to indemnify Cypress against all liability.
#********************************************************************************

#!/usr/bin/python

import base64
import hashlib
import hmac
import json
import urllib
import requests
import socket
import optparse
import time
import wave
import os
from datetime import datetime

DEFAULT_IP   = socket.gethostbyname(socket.gethostname())   # IP address of the UDP server
DEFAULT_PORT = 57345             # Port of the UDP server

# WAV文件参数
CHANNELS = 1                # 单声道
SAMPLE_WIDTH = 2           # 2字节 = 16位
FRAMERATE = 16000          # 采样率，常见值: 8000, 16000, 44100, 48000

# 结束标志位 - 当收到这个标志时表示数据传输结束
END_FLAG = b'flag'         # 对应客户端发送的结束标志


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
            return None

    except Exception as e:
        print(f"转写异常: {str(e)}")
        return None


def save_as_wav(buffer, filename, channels=CHANNELS, sample_width=SAMPLE_WIDTH, framerate=FRAMERATE):
    """将缓冲区数据保存为WAV文件"""
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(channels)           # 设置通道数
        wav_file.setsampwidth(sample_width)       # 设置采样宽度
        wav_file.setframerate(framerate)          # 设置帧率/采样率
        wav_file.writeframes(buffer)              # 写入音频数据
    return os.path.abspath(filename)


def echo_server(host, port):
    print("============================================================")
    print("UDP音频接收服务器")
    print("============================================================")
    # 创建UDP套接字
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # 绑定UDP服务器
    sock.bind((host, port))
    print('UDP服务器IP地址: {} 端口 {}'.format(host, port))
    print('等待接收来自UDP客户端的消息...')
    
    # 初始化变量
    buffer = bytearray()           # 用于保存所有接收到的数据
    client_addr = None             # 客户端地址
    packets_received = 0           # 收到的数据包数量
    recording = False              # 是否正在录制
    last_packet_time = None        # 最后一个数据包的时间
    
    try:
        while True:
            data, addr = sock.recvfrom(225000)
            current_time = time.time()
            
            # 检查是否是新的客户端或数据传输中断很长时间
            if not recording or client_addr != addr or \
               (last_packet_time and current_time - last_packet_time > 5):  # 5秒超时
                # 如果之前有录制，保存之前的数据
                if recording and len(buffer) > 0:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = f"audio_{timestamp}.wav"
                    filepath = save_as_wav(buffer, filename)
                    print(f"\n音频文件已保存: {filepath}")
                    print(f"接收到 {packets_received} 个数据包，总大小: {len(buffer)} 字节")
                
                # 重置录制状态
                buffer = bytearray()
                recording = True
                client_addr = addr
                packets_received = 0
                print(f"\n开始从 {addr[0]}:{addr[1]} 接收新的音频流")
            
            # 更新最后包接收时间
            last_packet_time = current_time
            
            # 检查是否是结束标志
            if data == END_FLAG:
                print(f"\n收到结束标志 '{END_FLAG.decode('latin-1')}'，正在保存音频文件...")
                
                # 如果缓冲区有数据，则保存音频文件
                if len(buffer) > 0:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = f"audio_{timestamp}.wav"
                    filepath = save_as_wav(buffer, filename)
                    print(f"音频文件已保存: {filepath}")
                    print(f"接收到 {packets_received} 个数据包，总大小: {len(buffer)} 字节")

                    text = audio2text(filepath)
                    print(f"语音识别返回：{text}")
                    if "救命" in text:
                        #TODO
                        print("用户求救")

                
                # 重置缓冲区和计数器
                buffer = bytearray()
                packets_received = 0
                print(f"缓冲区已重置，等待新的音频流")
            else:
                # 如果不是结束标志，则添加到缓冲区
                buffer.extend(data)
                packets_received += 1
                
                # 显示接收信息
                # print(f"接收到来自 {addr[0]}:{addr[1]} 的数据")
                # print(f"数据大小: {len(data)} 字节")
                # if len(data) > 0:
                #     print(f"数据前10个字节: {data[:10]}")
                # print(f"当前缓冲区大小: {len(buffer)} 字节 (已接收 {packets_received} 个数据包)")
            
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
        

if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option("-p", "--port", dest="port", type="int", default=DEFAULT_PORT, help="Port to listen on [default: %default].")
    parser.add_option("--hostname", dest="hostname", default=DEFAULT_IP, help="Hostname to listen on.")

    (options, args) = parser.parse_args()

    echo_server('192.168.10.117', options.port)
