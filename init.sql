CREATE TABLE trades (
    message_type      CHAR(1) NOT NULL,
    sequence_number   BIGINT  NOT NULL,
    trade_id          BIGINT  NOT NULL,
    timestamp         BIGINT  NOT NULL,              -- epoch in microseconds
    price             DOUBLE PRECISION NOT NULL,
    quantity          DOUBLE PRECISION NOT NULL,
    buyer_is_maker    BOOLEAN NOT NULL,
    best_match        BOOLEAN NOT NULL,
    symbol            VARCHAR(8) NOT NULL
);