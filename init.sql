CREATE TABLE trades (
    message_type CHAR(1),
    sequence_number BIGINT,
    trade_id BIGINT,
    timestamp BIGINT,
    price DOUBLE PRECISION,
    quantity DOUBLE PRECISION,
    buyer_is_maker BOOLEAN,
    best_match BOOLEAN,
    db_time TIMESTAMP
);
