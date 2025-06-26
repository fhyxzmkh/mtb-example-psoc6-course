import asyncio

from fastapi import FastAPI
import uvicorn
import udp
from api import init_api

app = FastAPI(title="嵌入式项目服务器", description="UDP音频接收服务器与前端通信API")

init_api(app)

@app.get("/")
async def root():
    return {"message": "嵌入式项目服务器已启动"}

# 在 FastAPI 应用程序启动时启动 UDP 服务器
@app.on_event("startup")
async def startup_event():
    print("FastAPI 应用程序启动中...")
    # 启动 UDP 服务器作为 asyncio 任务
    asyncio.create_task(udp.echo_server(udp.DEFAULT_IP, udp.DEFAULT_PORT))
    print("UDP 服务器已在后台启动。")

# 主函数
if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=9999)