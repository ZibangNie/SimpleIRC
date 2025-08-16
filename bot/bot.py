import socket
import random
import sys
from threading import Thread

# 假设的随机回复和命令处理
random_responses = [
    "I'm not sure what you mean by that...",
    "Did you know that elephants never forget?",
    "Let's not talk about that...",
    "Interesting fact: the human brain is only 2% of our body weight but consumes 20% of our energy!"
]


def connect_to_server(host, port, nickname, channel):
    irc = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
    irc.connect((host, port))
    irc.send(f"USER {nickname} {nickname} {nickname} :{nickname}\r\n".encode('utf-8'))
    irc.send(f"NICK {nickname}\r\n".encode('utf-8'))
    irc.send(f"JOIN {channel}\r\n".encode('utf-8'))

    # 监听消息
    listen_for_messages(irc, nickname, channel)


def listen_for_messages(irc, nickname, channel):
    while True:
        data = irc.recv(1024).decode('utf-8')
        if ":!hello" in data:
            send_message(irc, "Hello there!", data)
        elif ":!slap" in data:
            handle_slap_command(irc, nickname, channel, data)
        elif "PRIVMSG" in data and nickname in data:
            respond_to_private_message(irc, data)


def send_message(irc, message, data):
    # 简化版，仅发送消息到最后一个提到的频道
    channel = data.split()[-1] if data.startswith("PRIVMSG") else data.split()[2]
    irc.send(f"PRIVMSG {channel} :{message}\r\n".encode('utf-8'))


def handle_slap_command(irc, nickname, channel, data):
    parts = data.split()
    if len(parts) > 2 and parts[2].startswith(':'):
        target = parts[2][1:]
        if target == nickname:
            target = random.choice(get_channel_users(irc, channel))
        send_message(irc, f"@{target} just got slapped with a trout!", data)
    else:
        target = random.choice(get_channel_users(irc, channel))
        if target == nickname or target == parts[1].split('!')[0]:
            target = random.choice(get_channel_users(irc, channel))
        send_message(irc, f"@{target} just got slapped with a trout!", data)


def get_channel_users(irc, channel):
    # 注意：这个函数需要根据你的IRC服务器和协议的具体实现来编写
    # 这里只是一个占位符，因为直接从socket接收数据中解析用户列表通常很复杂
    return ["user1", "user2", "user3"]  # 示例用户列表


def respond_to_private_message(irc, data):
    # 简化处理：直接回复随机句子
    response = random.choice(random_responses)
    parts = data.split()
    to = parts[1].split('!')[0]
    irc.send(f"PRIVMSG {to} :{response}\r\n".encode('utf-8'))


def main():
    if len(sys.argv) < 5:
        print("Usage: python bot.py --host <host> --port <port> --name <nickname> --channel <channel>")
        sys.exit(1)

    args = sys.argv[1:]
    host = None
    port = 6667  # 默认端口，可以根据需要修改
    nickname = "DefaultBot"
    channel = "#default"

    for arg in args:
        if arg.startswith("--host"):
            host = arg.split('=')[1]
        elif arg.startswith("--port"):
            port = int(arg.split('=')[1])
        elif arg.startswith("--name"):
            nickname = arg.split('=')[1]
        elif arg.startswith("--channel"):
            channel = arg.split('=')[1]

    if not host:
        print("Host is required")
        sys.exit(1)

    connect_to_server(host, port, nickname, channel)


if __name__ == "__main__":
    main()