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
flags.DEFINE_enum('request_type', 'get', ['get', 'put'],
                  'get key from value or put new key value pair')
flags.DEFINE_string('key', 'Dog', 'key for get or put requests')
flags.DEFINE_string(
    'value', 'Findlay',
    'value for put requests. Ignored if the request type is get')


def construct_payload() -> bytes:
    if FLAGS.request_type == 'get':
        return FLAGS.key.encode('utf-8')

    key_header = struct.pack('!I', len(FLAGS.key))
    val_header = struct.pack('!I', len(FLAGS.value))
    key_bytes = FLAGS.key.encode('utf-8')
    val_bytes = FLAGS.value.encode('utf-8')

    return b"".join([key_header, key_bytes, val_header, val_bytes])


def send_payload(client_socket: socket.socket) -> None:

    print(f"Sending: {FLAGS.request_type} request")
    print(f"Request key: {FLAGS.key}")
    if FLAGS.request_type == 'put':
        print(f"Request value: {FLAGS.value}")

    payload_bytes = construct_payload()
    rpc_id = 0 if FLAGS.request_type == 'get' else 1
    size_header = struct.pack('!I', len(payload_bytes))
    rpc_id_header = struct.pack('!I', rpc_id)
    try:
        client_socket.sendall(size_header)
        client_socket.sendall(rpc_id_header)
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
