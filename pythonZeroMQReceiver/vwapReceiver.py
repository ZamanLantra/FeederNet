import zmq
import pandas as pd

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://localhost:5555")
socket.setsockopt_string(zmq.SUBSCRIBE, "")  # Subscribe to all messages

records = []
last_print_ts = None

try:
    while True:
        msg = socket.recv_string()
        symbol, ts_str, vwap_str = msg.split(",")
        ts = pd.to_datetime(int(ts_str), unit='ms')
        vwap = float(vwap_str)
        records.append({"timestamp": ts, "symbol": symbol, "VWAP": vwap})

        if last_print_ts is None or (ts - last_print_ts).total_seconds() >= 1.0:
            df = pd.DataFrame(records)
            print(df.head())
            print()
            records.clear()
            last_print_ts = ts

except KeyboardInterrupt:
    print("Exiting...")