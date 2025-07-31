import zmq
import pandas as pd

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5555")
socket.setsockopt_string(zmq.SUBSCRIBE, "")  # Subscribe to all messages

records = []

try:
    while True:
        msg = socket.recv_string()
        symbol, ts_str, vwap_str = msg.split(",")
        ts = pd.to_datetime(int(ts_str), unit='ms')
        vwap = float(vwap_str)
        records.append({"timestamp": ts, "symbol": symbol, "VWAP": vwap})

        if len(records) >= 1000:
            df = pd.DataFrame(records)
            print(df.head())
            records.clear()

except KeyboardInterrupt:
    print("Exiting...")
