import socket
import sys
import random
import argparse
import logging

# 配置日志记录系统
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

# 定义一些有趣的回应
FUN_RESPONSES = [
    "你好，我是bot！",
    "你知道吗？今天天气不错。",
    "42是宇宙的终极答案。",
    "为什么我们在这里？这是一个好问题。",
    "Leo Messi",
    "生活就像一盒巧克力，你永远不知道下一块是什么。"
    "What can I say?"
    "噔→噔↑噔↘噔→，噔噔噔噔噔噔噔→噔↑噔↘噔→"
]

# 定义bot的类
class IRCBot:
    def __init__(self, host, port, nickname, channel):
        self.host = host
        self.port = port
        self.nickname = nickname
        self.channel = channel
        self.socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
        self.users = []

    def connect(self):
        try:
            self.socket.connect((self.host, self.port))
            self.socket.send(f"NICK {self.nickname}\r\n".encode('utf-8'))
            self.socket.send(f"USER {self.nickname} 0 * :{self.nickname}\r\n".encode('utf-8'))
            print(f"Connected to {self.host} as {self.nickname}")
        except socket.error as e:#socket error
            print(f"Failed to connect: {e}")
            logging.error(f"Failed to connect to {self.host} on port {self.port} as {self.nickname}: {e}")
            self.cleanup()
            sys.exit(1)
        except Exception as e:
            print(f"An error occurred: {e}")
            logging.error(f"An unexpected error: {e}")
            self.cleanup()
            sys.exit(1)

    def cleanup(self):
        self.socket.close()
        self.socket = None
        print("Socket closed and cleaned up.")

    def listen(self):
        while True:
            try:
                data = self.socket.recv(4096).decode('utf-8')
                print(data)
                self.handle_data(data)
            except socket.error as e:
                print(f"Failed to receive data: {e}")
                break

    def handle_data(self, data):
        lines = data.split("\n")
        for line in lines:
            if line:
                tokens = line.split()
                command = tokens[0]
                if command == "PING":
                    self.socket.send(f"PONG {tokens[1]}\r\n".encode('utf-8'))
                elif command == "PRIVMSG":
                    self.handle_privmsg(tokens[2], " ".join(tokens[3:]))

    def handle_privmsg(self, target, message):
        commands = {
            "!hello": "你好！欢迎来到频道。",
            "!help": "我的功能包括：!hello, !slap, 和 !slap。",
            "!slap": "打耳光"
        }
        if message.startswith('!'):
            command, *args = message[1:].split(' ', 1)
            if command in commands:

                self.socket.send(f"PRIVMSG {target} :{commands[command]}\r\n".encode('utf-8'))
            elif command == "!slap":
                self.handle_slap(target, args[0] if args else None)
        else:

            self.socket.send(f"PRIVMSG {target} :{random.choice(FUN_RESPONSES)}\r\n".encode('utf-8'))

    def handle_slap(self, target, user=None):
        if not user or user not in self.users:
            users = [nick for nick in self.users if nick != self.nickname and nick != target]
            if users:
                victim = random.choice(users)
                self.socket.send(f"PRIVMSG {target} :{self.nickname} 打了 {victim}！\r\n".encode('utf-8'))
            else:
                self.socket.send(f"PRIVMSG {target} :{self.nickname} 看了看周围，没有人可以打。\r\n".encode('utf-8'))
        else:
            self.socket.send(f"PRIVMSG {target} :{self.nickname} 打了 {user} 一巴掌！\r\n".encode('utf-8'))

    def join_channel(self):
        self.socket.send(f"JOIN {self.channel}\r\n".encode('utf-8'))
        print(f"Joined channel {self.channel}")

    def run(self):
        self.connect()
        self.join_channel()
        self.listen()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="IRC Bot")
    parser.add_argument("--host", default="fe80::6d7:897c:c586:1fd4%8", help="Server host")
    parser.add_argument("--port", default=6667, type=int, help="Server port")
    parser.add_argument("--name", default="SuperBot", help="Nickname")
    parser.add_argument("--channel", default="#hello", help="Channel to join")
    args = parser.parse_args()

    bot = IRCBot(args.host, args.port, args.name, args.channel)
    bot.run()

# 代码说明：
# 初始化和连接：
#
# IRCBot 类初始化时，设置服务器地址、端口、昵称和频道。
# connect 方法用于连接到服务器并发送 NICK 和 USER 命令。
# listen 方法不断接收服务器发送的数据并处理。
# 数据处理：
#
# handle_data 方法处理接收到的数据，包括 PING 响应和 PRIVMSG 命令。
# handle_privmsg 方法处理私聊消息，包括 "!hello" 和 "!slap" 命令。
# handle_slap 方法处理 "!slap" 命令
# 加入频道：
#
# join_channel 方法用于加入指定的频道。
# 运行：
#
# run 启动机器人，连接服务器，加入频道并开始监听。
# 命令行参数：
#
# 使用 argparse 处理命令行参数，包括服务器地址、端口、昵称和频道。
# 运行环境：
# 确保在 Windows 虚拟机中运行此 Python 脚本，并连接到 Ubuntu 虚拟机上的 IRC 服务器。
# 使用 IPv6 地址进行连接。
