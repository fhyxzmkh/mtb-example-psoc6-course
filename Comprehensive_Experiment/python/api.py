from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from typing import List, Dict, Any
import json
import asyncio
from pydantic import BaseModel

# 创建一个WebSocket连接管理器
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        print(f"WebSocket connected: {websocket.client}") # 增加连接日志

    def disconnect(self, websocket: WebSocket):
        # 确保连接在列表中
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            print(f"WebSocket disconnected: {websocket.client}") # 增加断开日志

    async def send_json(self, message: Dict[str, Any]):
        # 创建一个临时列表来存储需要移除的连接
        connections_to_remove = []
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except WebSocketDisconnect:
                # 捕获断开连接异常，将此连接标记为待移除
                print(f"尝试向已断开的WebSocket连接发送消息失败: {connection.client}. 将移除该连接。")
                connections_to_remove.append(connection)
            except Exception as e:
                # 捕获其他可能的发送错误
                print(f"发送WebSocket消息时发生未知错误到 {connection.client}: {e}")
                connections_to_remove.append(connection) # 同样标记为待移除，以防是持续性问题

        # 在循环结束后一次性移除所有断开的连接
        for connection in connections_to_remove:
            self.disconnect(connection) # 使用已有的 disconnect 方法

# 创建连接管理器实例
manager = ConnectionManager()

# 数据模型
class AudioTextResponse(BaseModel):
    text: str

class ControlResponse(BaseModel):
    number: int

# 初始化API路由
def init_api(app: FastAPI):
    @app.websocket("/ws")
    async def websocket_endpoint(websocket: WebSocket):
        await manager.connect(websocket)
        try:
            while True:
                # 保持连接活跃
                await asyncio.sleep(1)
        except WebSocketDisconnect:
            manager.disconnect(websocket)

    @app.get("/api/control/trend/rising")
    async def trend_rising():
        """上升趋势检测 API"""
        await manager.send_json({"number": 3})
        return {"number": 3, "message": "Rising trend detected"}

    @app.get("/api/control/trend/falling")
    async def trend_falling():
        """下降趋势检测 API"""
        await manager.send_json({"number": 2})
        return {"number": 2, "message": "Falling trend detected"}

    @app.get("/api/control/start")
    async def control_start():
        """开始命令 API"""
        await manager.send_json({"number": 1})
        return {"number": 1, "message": "Start command received"}

    @app.get("/api/control/stop")
    async def control_stop():
        """停止命令 API"""
        await manager.send_json({"number": 0})
        return {"number": 0, "message": "Stop command received"}

    @app.get("/api/control/invalid")
    async def control_invalid():
        """无效控制数据 API"""
        await manager.send_json({"number": -1})
        return {"number": -1, "message": "Invalid control data"}

    @app.post("/api/audio/text")
    async def audio_text(response: AudioTextResponse):
        """语音识别结果 API"""
        await manager.send_json({"text": response.text})
        return {"text": response.text, "message": "Audio transcription completed"}

# 提供直接调用的函数，用于从UDP服务器发送数据到WebSocket客户端
async def send_trend_rising():
    await manager.send_json({"number": 3})

async def send_trend_falling():
    await manager.send_json({"number": 2})

async def send_control_start():
    await manager.send_json({"number": 1})

async def send_control_stop():
    await manager.send_json({"number": 0})

async def send_control_invalid():
    await manager.send_json({"number": -1})

async def send_audio_text(text: str):
    await manager.send_json({"text": text})
