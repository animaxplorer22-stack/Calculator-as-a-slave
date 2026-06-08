#!/usr/bin/env python3
"""
DUCO Calculator Bridge - Runs on computer
Connects TI-84 Plus CE to Duino-Coin network via USB serial
"""

import serial
import serial.tools.list_ports
import socket
import time
import sys

# ==================== CONFIGURATION ====================
DUCO_USER = "YOUR_USERNAME_HERE"     # CHANGE THIS
MINING_KEY = ""                       # Leave empty unless you have one
RIG_NAME = "CalculatorMiner"
COMPUTER_PORT = "COM3"                # Windows: COM#, Mac/Linux: /dev/ttyUSB0
# ========================================================

SERVER = "server.duinocoin.com"
PORT = 2811
VERSION = "CalculatorBridge/v1.0"

def find_calculator():
    """Auto-detect calculator USB serial port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "TI-84" in port.description or "Texas Instruments" in port.description:
            return port.device
    return COMPUTER_PORT  # Fallback to manual config

def connect_serial():
    """Connect to calculator over USB"""
    port = find_calculator()
    print(f"Connecting to calculator on {port}...")
    
    try:
        ser = serial.Serial(port, 115200, timeout=10)
        time.sleep(2)
        
        # Test connection
        ser.write(b"PING\n")
        start = time.time()
        while time.time() - start < 5:
            if ser.in_waiting:
                response = ser.readline().decode().strip()
                if response == "PONG":
                    print("✅ Calculator ready!")
                    return ser
                break
        
        print("❌ Calculator not responding")
        print("   Make sure the code is uploaded and USB cable is connected")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Failed to connect: {e}")
        sys.exit(1)

def recv_response(sock, timeout=10):
    """Read full response from DUCO server"""
    sock.settimeout(timeout)
    data = b""
    start = time.time()
    
    while time.time() - start < timeout:
        try:
            chunk = sock.recv(1)
            if chunk:
                data += chunk
                if data.endswith(b'\n'):
                    break
            else:
                time.sleep(0.01)
        except socket.timeout:
            break
    
    return data.decode().strip()

def main():
    print("=" * 50)
    print("  DUCO Calculator Bridge")
    print("=" * 50)
    print(f"User: {DUCO_USER}")
    print(f"Rig: {RIG_NAME}")
    
    if DUCO_USER == "YOUR_USERNAME_HERE":
        print("❌ ERROR: Change DUCO_USER to your username!")
        sys.exit(1)
    
    # Connect to calculator
    calc = connect_serial()
    
    # Connect to DUCO server
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((SERVER, PORT))
        print(f"✅ Connected to {SERVER}:{PORT}")
    except Exception as e:
        print(f"❌ Cannot connect to server: {e}")
        sys.exit(1)
    
    accepted = 0
    rejected = 0
    
    print("\n⛏️ Mining started! Press Ctrl+C to stop\n")
    
    try:
        while True:
            # Get job from server
            job_request = f"JOB,{DUCO_USER},{MINING_KEY}\n"
            sock.send(job_request.encode())
            
            job_data = recv_response(sock, 10)
            if not job_data:
                print("No job received, retrying...")
                time.sleep(5)
                continue
            
            parts = job_data.split(',')
            if len(parts) >= 3:
                last_hash = parts[0]
                expected = parts[1]
                difficulty = parts[2]
                
                print(f"📡 Job: diff={difficulty}, target={expected[:16]}...")
                
                # Send to calculator
                calc_cmd = f"JOB,{last_hash},{expected},{difficulty}\n"
                calc.write(calc_cmd.encode())
                
                # Wait for result
                result = ""
                start = time.time()
                while time.time() - start < 60:  # Calculator is slower
                    if calc.in_waiting:
                        result = calc.readline().decode().strip()
                        break
                    time.sleep(0.1)
                
                if result.startswith("FOUND"):
                    nonce = result.split(',')[1]
                    
                    # Submit share
                    share_request = f"{nonce},0,{VERSION},{RIG_NAME}\n"
                    sock.send(share_request.encode())
                    
                    response = recv_response(sock, 10)
                    
                    if response.startswith("GOOD"):
                        accepted += 1
                        print(f"✅ ACCEPTED! (A:{accepted} R:{rejected})")
                    else:
                        rejected += 1
                        print(f"❌ REJECTED: {response}")
                        
                elif result == "NONCE_NOT_FOUND":
                    print("⏳ No nonce, getting new job...")
                else:
                    print(f"⚠️ Unknown response: {result}")
                    
    except KeyboardInterrupt:
        print(f"\n🛑 Stopping. Final - A:{accepted} R:{rejected}")
    finally:
        calc.close()
        sock.close()

if __name__ == "__main__":
    main()
