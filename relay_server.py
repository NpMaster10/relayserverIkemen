import socket
import threading
import queue
import signal
import sys

PORT = 7500
HOST = '0.0.0.0'
pending_clients = queue.Queue()

def relay(conn1, conn2):
    def forward(src, dst):
        try:
            while True:
                data = src.recv(4096)
                if not data:
                    break
                dst.sendall(data)
        except Exception as e:
            print(f"Relay error: {e}")
        finally:
            try: src.shutdown(socket.SHUT_RD)
            except: pass
            try: dst.shutdown(socket.SHUT_WR)
            except: pass
            src.close()
            dst.close()

    threading.Thread(target=forward, args=(conn1, conn2), daemon=True).start()
    threading.Thread(target=forward, args=(conn2, conn1), daemon=True).start()


def handle_client(conn):
    try:
        print(f"Client {conn.fileno()} connected.")
        if pending_clients.empty():
            print(f"Client {conn.fileno()} waiting for a pair.")
            pending_clients.put(conn)
        else:
            conn2 = pending_clients.get()
            print(f"Pairing client {conn.fileno()} with {conn2.fileno()}.")
            relay(conn, conn2)
    except Exception as e:
        print(f"Error: {e}")
        conn.close()

def main():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, PORT))
    server_socket.listen()
    print(f"Relay server listening on {PORT}...")

    def shutdown(signum, frame):
        print("\nShutting down server...")
        server_socket.close()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)   # Handle Ctrl+C
    signal.signal(signal.SIGTERM, shutdown)  # Handle kill

    try:
        while True:
            conn, addr = server_socket.accept()
            print(f"Accepted connection from {addr}")
            threading.Thread(target=handle_client, args=(conn,), daemon=True).start()
    except Exception as e:
        print(f"Server error: {e}")
    finally:
        server_socket.close()
        print("Server socket closed.")


if __name__ == '__main__':
    main()

