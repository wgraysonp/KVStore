import socket
import sys
import traceback
import struct
import typing

from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string('server_IP', '127.0.0.1', 'The server IP address.')
flags.DEFINE_integer('server_port', 8080, 'Port the server is listening on.')
flags.DEFINE_string('name', 'Findlay', 'Name to send to the server.')

def send_payload(client_socket: socket.socket) -> None:
    payload = FLAGS.name
    print(f"Sending: {payload}")
    payload_bytes = payload.encode('utf-8')
    header = struct.pack('!I', len(payload_bytes))
    try: 
        client_socket.sendall(header)
        client_socket.sendall(payload_bytes)
    except Exception as e:
        raise

def read_server_response(client_socket) -> str:
    size_header = client_socket.recv(4)
    if len(size_header) < 4:
        raise RuntimeError("Failed to read server header")
    
    payload_size = struct.unpack('!I', size_header)[0]
    print(f"Server says payload size will be: {payload_size} bytes")
    
    response_bytes = b""
    while len(response_bytes) < payload_size:
        try:
            packet = client_socket.recv(payload_size - len(response_bytes))
            if not packet:
                raise ConnectionError("Server disconnected prematurely.")
            response_bytes += packet
        except Exception as e:
            raise
    
    response_text = response_bytes.decode('utf-8')
    return response_text




def main(argv: list[str]) -> None:
    del argv

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        print(f"Connecting to {FLAGS.server_IP}:{FLAGS.server_port}...")
        client_socket.connect((FLAGS.server_IP, FLAGS.server_port))

        send_payload(client_socket)

        response_text = read_server_response(client_socket)

        print(f"Recieved from server: {response_text}")

    except Exception as e:
        print(f"\n=== CRITICAL NETWORK ERROR ===")
        print(f"Error Type: {type(e).__name__}")
        print(f"Error Message: {e}")
        if hasattr(e, 'errno'):
            print(f"OS Error Number (errno): {e.errno}")
        print("\n--- Full Python Stack Trace ---")
        traceback.print_exc()
        print("===============================\n")

    finally:
        client_socket.close()
        print("Connection closed.")


if __name__ == "__main__":
    app.run(main)
