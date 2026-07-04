#!/usr/bin/env python3
"""
OTA TCP Server - 通过 TCP 向 ESP8266 发送固件实现 OTA 升级

数据流:
  PC (TCP Server) <--WiFi/TCP--> ESP8266 <--UART--> STM32 (iap_receive)

工作模式:
  1. 标准模式 (默认): 等待设备发送 0xFF 0x57 0x41 0x56 握手后按协议交互
  2. 主动模式 (--push) : 不等待握手，直接推送固件大小 + 数据

用法:
  python ota.py <固件.bin> [--port PORT] [--push]

示例:
  # 标准模式: 等待设备握手后发送
  python ota.py firmware.bin --port 6000

  # 主动模式: 连接后直接推送
  python ota.py firmware.bin --port 6000 --push

ESP8266 端 AT 配置 (透传模式):
  AT+CWMODE=1                  # STA 模式
  AT+CWJAP="SSID","PASSWORD"  # 连接 WiFi
  AT+CIPSTART="TCP","SERVER_IP",6000  # 连接服务器
  AT+CIPMODE=1                 # 开启透传模式
  AT+CIPSEND                   # 进入透传
  # 之后设备 iap_receive() 会自动发起握手交互
"""

import sys
import os
import struct
import time
import socket
import argparse

# ============================================================
# 协议常量
# ============================================================
HANDSHAKE_CMD = bytes([0xFF, 0x57, 0x41, 0x56])  # 设备握手信号
CHUNK_SIZE = 256                                    # 每包数据大小
DEFAULT_PORT = 6000                                 # 默认端口


class OTATcpServer:
    """OTA TCP 升级服务器"""

    def __init__(self, firmware_path: str, port: int = DEFAULT_PORT, push_mode: bool = False):
        """
        :param firmware_path: .bin 固件文件路径
        :param port:          TCP 服务器监听端口
        :param push_mode:     主动推送模式 (True=不等待握手, False=标准协议)
        """
        self.firmware_path = firmware_path
        self.port = port
        self.push_mode = push_mode
        self.firmware = b""
        self.firmware_size = 0
        self.server_socket: socket.socket | None = None
        self.client_socket: socket.socket | None = None
        self.client_addr: tuple | None = None
        self.rx_buffer = bytearray()
        self._running = False

    def load_firmware(self) -> bool:
        """加载固件文件"""
        try:
            with open(self.firmware_path, 'rb') as f:
                self.firmware = f.read()
        except FileNotFoundError:
            print(f"[错误] 固件文件不存在: {self.firmware_path}")
            return False
        except IOError as e:
            print(f"[错误] 读取固件文件失败: {e}")
            return False

        self.firmware_size = len(self.firmware)
        if self.firmware_size == 0:
            print("[错误] 固件文件为空")
            return False

        max_size = 1024 * 1024  # 1MB (W25QXX A区大小)
        if self.firmware_size > max_size:
            print(f"[警告] 固件大小 ({self.firmware_size/1024:.1f}KB) 超过 1MB，可能超出 A 区容量!")

        print(f"[固件] {self.firmware_path}")
        print(f"[固件] 大小: {self.firmware_size} 字节 ({self.firmware_size/1024:.1f} KB)")
        return True

    def start(self) -> bool:
        """启动 TCP 服务器"""
        if not self.load_firmware():
            return False

        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(('0.0.0.0', self.port))
            self.server_socket.listen(1)
            self.server_socket.settimeout(1.0)
            self._running = True

            print(f"\n{'='*50}")
            print(f"  OTA TCP 服务器启动")
            print(f"  监听端口: {self.port}")
            print(f"  工作模式: {'主动推送' if self.push_mode else '标准协议'}")
            print(f"  等待 ESP8266 连接...")
            print(f"{'='*50}")
            return True
        except OSError as e:
            print(f"[错误] 启动服务器失败: {e}")
            return False

    def stop(self):
        """停止服务器"""
        self._running = False
        if self.client_socket:
            try:
                self.client_socket.close()
            except:
                pass
            self.client_socket = None
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
            self.server_socket = None
        print("\n[信息] 服务器已停止")

    def wait_for_client(self) -> bool:
        """等待 ESP8266 客户端连接"""
        while self._running:
            try:
                client_sock, addr = self.server_socket.accept()
                self.client_socket = client_sock
                self.client_addr = addr
                print(f"\n[连接] ESP8266 已连接: {addr[0]}:{addr[1]}")
                return True
            except socket.timeout:
                continue
            except OSError as e:
                if self._running:
                    print(f"[错误] 接受连接失败: {e}")
                return False
        return False

    def recv_all(self, length: int, timeout: float = 10.0) -> bytes:
        """接收指定长度的数据"""
        deadline = time.monotonic() + timeout
        while len(self.rx_buffer) < length and self._running:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                self.client_socket.settimeout(min(remaining, 1.0))
                chunk = self.client_socket.recv(4096)
                if not chunk:
                    print("\n[断开] ESP8266 连接已断开")
                    return b""
                self.rx_buffer.extend(chunk)

                # 打印收到的文本数据（调试信息）
                try:
                    text = chunk.decode('utf-8', errors='ignore').strip()
                    if text and not all(b in [0xFF, 0x57, 0x41, 0x56] for b in chunk):
                        print(f"[设备] {text}")
                except:
                    pass

            except socket.timeout:
                continue
            except OSError as e:
                print(f"\n[错误] 接收数据失败: {e}")
                return b""

        if len(self.rx_buffer) >= length:
            data = bytes(self.rx_buffer[:length])
            self.rx_buffer = self.rx_buffer[length:]
            return data
        return b""

    def send_all(self, data: bytes) -> bool:
        """发送所有数据"""
        try:
            self.client_socket.sendall(data)
            return True
        except OSError as e:
            print(f"\n[错误] 发送数据失败: {e}")
            return False

    def wait_for_handshake(self, timeout: float = 60.0) -> bool:
        """等待设备握手信号 [0xFF, 0x57, 0x41, 0x56]"""
        print("\n[等待] 等待设备握手信号 (0xFF 0x57 0x41 0x56)...")
        start = time.monotonic()

        while time.monotonic() - start < timeout and self._running:
            # 在缓冲区中查找握手信号
            idx = -1
            for i in range(len(self.rx_buffer) - 3):
                if self.rx_buffer[i:i+4] == HANDSHAKE_CMD:
                    idx = i
                    break

            if idx >= 0:
                if idx > 0:
                    print(f"[信息] 丢弃 {idx} 字节前置数据")
                self.rx_buffer = self.rx_buffer[idx + 4:]
                elapsed = time.monotonic() - start
                print(f"[OK] 收到握手信号 ({elapsed:.1f}s)")
                return True

            # 读取更多数据
            try:
                self.client_socket.settimeout(0.5)
                chunk = self.client_socket.recv(4096)
                if not chunk:
                    print("\n[断开] ESP8266 连接已断开")
                    return False
                self.rx_buffer.extend(chunk)

                try:
                    text = chunk.decode('utf-8', errors='ignore').strip()
                    if text:
                        print(f"[设备] {text}")
                except:
                    pass

            except socket.timeout:
                continue
            except OSError as e:
                print(f"\n[错误] {e}")
                return False

        print(f"\n[超时] {timeout}秒内未收到握手信号")
        return False

    def send_firmware_size(self) -> bool:
        """发送固件大小 (4字节, 大端序)"""
        data = struct.pack('>I', self.firmware_size)
        if not self.send_all(data):
            return False
        print(f"[发送] 固件大小: {self.firmware_size} 字节 ({self.firmware_size/1024:.1f} KB)")
        return True

    def send_firmware_data(self) -> bool:
        """按 256 字节分片发送固件数据"""
        total_chunks = (self.firmware_size + CHUNK_SIZE - 1) // CHUNK_SIZE
        sent_bytes = 0

        print(f"\n[发送] 开始发送固件数据 (共 {total_chunks} 包)...")
        start_time = time.monotonic()

        for chunk_idx in range(total_chunks):
            if not self._running:
                print("\n[中断] 升级被中断")
                return False

            # 每一包都等待设备握手（包括第1包）
            if not self.push_mode:
                if not self.wait_for_handshake(timeout=15):
                    print(f"\n[错误] 第 {chunk_idx+1}/{total_chunks} 包等待握手超时")
                    return False

            # 构造数据块
            offset = chunk_idx * CHUNK_SIZE
            chunk = self.firmware[offset:offset + CHUNK_SIZE]
            if len(chunk) < CHUNK_SIZE:
                chunk = chunk + bytes([0xFF] * (CHUNK_SIZE - len(chunk)))

            # 发送
            if not self.send_all(chunk):
                return False

            sent_bytes += min(CHUNK_SIZE, self.firmware_size - offset)

            # 进度
            progress = (chunk_idx + 1) * 100 // total_chunks
            elapsed = time.monotonic() - start_time
            speed = sent_bytes / elapsed if elapsed > 0 else 0
            print(f"\r[进度] {progress}% ({chunk_idx+1}/{total_chunks}) "
                  f"{sent_bytes/1024:.1f}KB | {speed/1024:.1f}KB/s", end="")

        print()
        elapsed = time.monotonic() - start_time
        print(f"\n[完成] 固件发送完毕! 耗时: {elapsed:.1f}s")
        return True

    def wait_for_ack(self, timeout: float = 20.0) -> bool:
        """等待设备升级完成确认"""
        print("[等待] 等待设备完成写入...")
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline and self._running:
            try:
                self.client_socket.settimeout(0.5)
                chunk = self.client_socket.recv(4096)
                if not chunk:
                    print("\n[断开] ESP8266 连接已断开")
                    return False

                text = chunk.decode('utf-8', errors='ignore')
                print(f"[设备] {text}", end="")

                if "iap load ok" in text or "iap update ok" in text:
                    print()
                    return True
            except socket.timeout:
                continue
            except OSError:
                return False

        return False

    def handle_client(self):
        """处理客户端 OTA 升级"""
        if not self.client_socket:
            return

        print("\n" + "=" * 50)
        print("    开始 OTA 固件升级")
        print("=" * 50)

        self.rx_buffer.clear()

        try:
            if self.push_mode:
                # ---- 主动推送模式 ----
                print("[模式] 主动推送模式")
                time.sleep(0.5)

                if not self.send_firmware_size():
                    return
                time.sleep(0.1)

                if not self.send_firmware_data():
                    return

            else:
                # ---- 标准协议模式 ----
                print("[模式] 标准协议模式 (等待握手)")

                if not self.wait_for_handshake():
                    return

                if not self.send_firmware_size():
                    return
                time.sleep(0.05)

                if not self.send_firmware_data():
                    return

            # 等待设备确认
            ack_ok = self.wait_for_ack()

            if ack_ok:
                print("\n" + "=" * 50)
                print("    OTA 升级成功! 设备重启后生效")
                print("=" * 50)
            else:
                print("\n[警告] 未收到设备确认，请检查设备状态")

        finally:
            try:
                self.client_socket.close()
            except:
                pass
            self.client_socket = None
            self.rx_buffer.clear()

    def run(self):
        """运行服务器（接受连接并处理）"""
        if not self.start():
            return

        try:
            if self.wait_for_client():
                self.handle_client()
        except KeyboardInterrupt:
            print("\n\n[信息] 用户中断")
        finally:
            self.stop()


def main():
    parser = argparse.ArgumentParser(
        description="OTA TCP Server - 通过 TCP 向 ESP8266 发送固件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
ESP8266 端配置示例 (透传模式):
  AT+CWMODE=1
  AT+CWJAP="WiFi名称","WiFi密码"
  AT+CIPSTART="TCP","服务器IP",6000
  AT+CIPMODE=1
  AT+CIPSEND

设备端 (STM32) 接到透传数据后自动回复 iap_receive() 协议
"""
    )
    parser.add_argument("firmware", help=".bin 固件文件路径")
    parser.add_argument("--port", "-p", type=int, default=DEFAULT_PORT,
                        help=f"TCP 监听端口 (默认: {DEFAULT_PORT})")
    parser.add_argument("--push", action="store_true",
                        help="主动推送模式: 不等待握手，连接后直接发送")

    args = parser.parse_args()

    if not os.path.exists(args.firmware):
        print(f"[错误] 文件不存在: {args.firmware}")
        sys.exit(1)

    server = OTATcpServer(
        firmware_path=args.firmware,
        port=args.port,
        push_mode=args.push
    )
    server.run()


if __name__ == "__main__":
    main()