#!/usr/bin/env python3
"""
MQTT OTA 升级工具 - 设备主导 + hex 纯数据

流程:
  1) PC 等待设备发 START_REQ (topic: device/ota/ack)
  2) PC 发布 start → payload = 固件字节数 (十进制纯数字)
  3) PC 等待设备 "OK"
  4) PC 逐包发布 data → payload = hex 字符串 (无包序号)
     每包等设备 "OK" (超时重试)
  5) PC 发布 stop (CRC)
  6) PC 等待设备 "OK"

用法:
  python ota_mqtt.py firmware.bin
  python ota_mqtt.py firmware.bin --host broker.emqx.io --port 1883
"""

import sys
import time
import argparse
import binascii
import threading

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[错误] 需要安装 paho-mqtt 库: pip install paho-mqtt")
    sys.exit(1)

# ============================================================
TOPIC_START = "device/ota/start"
TOPIC_SIZE  = "device/ota/size"
TOPIC_DATA  = "device/ota/data"
TOPIC_STOP  = "device/ota/stop"
TOPIC_ACK   = "device/ota/ack"

CHUNK_RAW_SIZE  = 1024              # PC 每包 1024 B → 2048 hex 字符
                                    # (ESP8266 会再拆成多条 SUBRECV)
ACK_TIMEOUT     = 15.0              # 等 ACK 超时 (秒)
MAX_RETRIES     = 3                 # 每包重试
START_REQ_TIMEOUT = 60.0            # 等设备发起请求超时


class OTAPushClient:
    def __init__(self, firmware_path: str,
                 host: str = "broker.emqx.io",
                 port: int = 1883,
                 client_id: str = "ota_publisher"):
        self.firmware_path = firmware_path
        self.mqtt_host = host
        self.mqtt_port = port
        self.client_id = client_id
        self.firmware = b""
        self.firmware_size = 0
        self.client = None
        self._connected = False
        self._ack_event = threading.Event()
        self._last_ack = ""

    # ------------------------------------------------------------
    def load_firmware(self) -> bool:
        try:
            with open(self.firmware_path, 'rb') as f:
                self.firmware = f.read()
        except FileNotFoundError:
            print(f"[错误] 固件文件不存在: {self.firmware_path}")
            return False
        except IOError as e:
            print(f"[错误] 读取固件失败: {e}")
            return False

        self.firmware_size = len(self.firmware)
        if self.firmware_size == 0:
            print("[错误] 固件为空")
            return False

        crc32 = binascii.crc32(self.firmware) & 0xFFFFFFFF
        print(f"[固件] {self.firmware_path}")
        print(f"[固件] 大小: {self.firmware_size} 字节 ({self.firmware_size/1024:.1f} KB)")
        print(f"[固件] CRC32: {crc32:08X}")
        return True

    # ------------------------------------------------------------
    def _on_connect(self, client, userdata, flags, rc, *args, **kwargs):
        if rc == 0:
            self._connected = True
            print(f"[MQTT] 连接成功 ({self.mqtt_host}:{self.mqtt_port})")
            self.client.subscribe(TOPIC_ACK, qos=0)
            print(f"[MQTT] 已订阅 {TOPIC_ACK}")
        else:
            print(f"[错误] MQTT 连接失败, 返回码: {rc}")

    def _on_disconnect(self, client, userdata, rc, *args, **kwargs):
        self._connected = False

    def _on_message(self, client, userdata, msg):
        self._last_ack = msg.payload.decode('utf-8', errors='ignore').strip()
        self._ack_event.set()

    # ------------------------------------------------------------
    def connect(self) -> bool:
        try:
            self.client = mqtt.Client(client_id=self.client_id,
                                      protocol=mqtt.MQTTv311)
            self.client.on_connect = self._on_connect
            self.client.on_disconnect = self._on_disconnect
            self.client.on_message = self._on_message

            print(f"\n{'='*55}")
            print(f"  MQTT OTA 推送 (设备主导)")
            print(f"  服务器:  {self.mqtt_host}:{self.mqtt_port}")
            print(f"  每包:    {CHUNK_RAW_SIZE} 原始字节 → {CHUNK_RAW_SIZE*2} hex 字符")
            print(f"  ACK超时: {ACK_TIMEOUT}s  重试: {MAX_RETRIES}次")
            print(f"{'='*55}")

            self.client.connect(self.mqtt_host, self.mqtt_port, keepalive=60)
            self.client.loop_start()

            for _ in range(100):
                if self._connected:
                    time.sleep(0.3)
                    return True
                time.sleep(0.1)
            print("[错误] MQTT 连接超时")
            return False
        except Exception as e:
            print(f"[错误] MQTT 连接异常: {e}")
            return False

    def disconnect(self):
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()

    def _wait_ack(self, timeout: float = ACK_TIMEOUT) -> str:
        self._ack_event.clear()
        if self._ack_event.wait(timeout):
            return self._last_ack
        return ""

    # ------------------------------------------------------------
    def run(self) -> bool:
        if not self._connected:
            print("[错误] MQTT 未连接")
            return False

        total_chunks = (self.firmware_size + CHUNK_RAW_SIZE - 1) // CHUNK_RAW_SIZE

        # ============================================================
        # 第 1 步: 等待设备发起 START_REQ
        # ============================================================
        print(f"\n[等待] 设备发起升级请求 ({TOPIC_ACK} + START_REQ)...")
        ack = self._wait_ack(START_REQ_TIMEOUT)
        if ack != "START_REQ":
            print(f"[错误] 未收到设备请求 (收到: {ack})")
            return False
        print("[设备] 收到 START_REQ")

        # ============================================================
        # 第 2 步: 发布 start → 纯十进制数字（MCU 用 sscanf %lu 解析）
        # ============================================================
        print(f"[发送] start: {self.firmware_size}")
        self.client.publish(TOPIC_START, str(self.firmware_size), qos=1)

        ack = self._wait_ack()
        if ack != "OK":
            print(f"[错误] 设备未就绪 (收到: {ack})")
            return False
        print("[设备] 就绪, 开始传输")

        # ============================================================
        # 第 3 步: 逐包 data → 纯 hex（MCU 跳过 3 逗号直接解码）
        # ============================================================
        print(f"[发送] 共 {total_chunks} 包...")
        offset = 0
        chunk_num = 0
        retries = 0

        while offset < self.firmware_size:
            chunk = self.firmware[offset:offset + CHUNK_RAW_SIZE]
            raw_size = len(chunk)  # 当前包的实际字节数
            hex_payload = chunk.hex().upper()   # ★ 无包序号前缀

            # 先发 size，带重试
            for retry in range(MAX_RETRIES + 1):
                self.client.publish(TOPIC_SIZE, str(raw_size), qos=1)
                ack = self._wait_ack()
                if ack == "OK":
                    break
                print(f"\n[重试] size#{chunk_num} ({retry + 1}/{MAX_RETRIES})...")
                time.sleep(1)
            else:
                print(f"\n[错误] size#{chunk_num} 重试 {MAX_RETRIES} 次失败")
                return False

            # 再发送数据
            self.client.publish(TOPIC_DATA, hex_payload, qos=1)

            ack = self._wait_ack()
            if ack == "OK":
                retries = 0
                offset += raw_size
                chunk_num += 1

                pct = min(offset, self.firmware_size) * 100 // self.firmware_size
                bar_len = 30
                filled = bar_len * min(offset, self.firmware_size) // self.firmware_size
                bar = '█' * filled + '░' * (bar_len - filled)
                print(f"\r  [{bar}] {pct}%  pkt#{chunk_num}/{total_chunks}", end="", flush=True)
            else:
                retries += 1
                if retries > MAX_RETRIES:
                    print(f"\n[错误] 包#{chunk_num} 重试 {MAX_RETRIES} 次失败")
                    return False
                print(f"\n[重试] 包#{chunk_num} ({retries}/{MAX_RETRIES})...")
                time.sleep(1)

        print()

        # ============================================================
        # 第 4 步: 发布 stop + 等最终 OK
        # ============================================================
        crc32 = binascii.crc32(self.firmware) & 0xFFFFFFFF
        print(f"[发送] stop: CRC={crc32:08X}")
        self.client.publish(TOPIC_STOP, f"{crc32:08X}", qos=1)

        ack = self._wait_ack(timeout=30.0)
        if ack == "OK":
            print("[设备] 升级完成!")
        else:
            print(f"[警告] 最终 ACK 异常: {ack}")

        # ============================================================
        print(f"\n{'='*55}")
        print(f"  OTA {'成功' if ack == 'OK' else '失败'}!")
        print(f"  大小: {self.firmware_size} 字节 ({total_chunks} 包)")
        print(f"  CRC: {crc32:08X}")
        print(f"{'='*55}")
        return ack == "OK"


def main():
    parser = argparse.ArgumentParser(description="MQTT OTA (设备主导, hex 纯数据)")
    parser.add_argument("firmware", help="固件 .bin 路径")
    parser.add_argument("--host", default="broker.emqx.io")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--client-id", default="ota_publisher")
    args = parser.parse_args()

    ota = OTAPushClient(args.firmware, args.host, args.port, args.client_id)
    if not ota.load_firmware():
        sys.exit(1)
    if not ota.connect():
        ota.disconnect()
        sys.exit(1)

    try:
        ok = ota.run()
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[信息] 用户中断")
    finally:
        ota.disconnect()
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()