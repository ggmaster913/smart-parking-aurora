-- Начальная схема приложения. Выполнять один раз для user_version = 0.
-- Перед применением включить PRAGMA foreign_keys = ON на connection.
-- При ошибке исполнитель обязан выполнить ROLLBACK.
BEGIN IMMEDIATE;

CREATE TABLE users (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    contact TEXT NOT NULL UNIQUE CHECK (length(contact) BETWEEN 1 AND 254),
    display_name TEXT NOT NULL DEFAULT ''
);

CREATE TABLE vehicles (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    user_id TEXT NOT NULL REFERENCES users(id),
    plate TEXT NOT NULL CHECK (length(plate) BETWEEN 4 AND 15),
    brand TEXT NOT NULL CHECK (length(brand) BETWEEN 1 AND 60),
    UNIQUE (user_id, plate),
    UNIQUE (id, user_id)
);

CREATE TABLE settings (
    key TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL
);

CREATE TABLE zones (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    name TEXT NOT NULL,
    address TEXT NOT NULL,
    latitude REAL NOT NULL CHECK (latitude BETWEEN -90 AND 90),
    longitude REAL NOT NULL CHECK (longitude BETWEEN -180 AND 180),
    rate_kopecks_per_hour INTEGER NOT NULL
        CHECK (typeof(rate_kopecks_per_hour) = 'integer' AND rate_kopecks_per_hour >= 0),
    working_hours TEXT NOT NULL
);

CREATE TABLE spaces (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    zone_id TEXT NOT NULL REFERENCES zones(id),
    label TEXT NOT NULL,
    mock_occupied INTEGER NOT NULL DEFAULT 0 CHECK (mock_occupied IN (0, 1)),
    UNIQUE (zone_id, label)
);

CREATE TABLE sessions (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    user_id TEXT NOT NULL REFERENCES users(id),
    vehicle_id TEXT NOT NULL,
    space_id TEXT NOT NULL REFERENCES spaces(id),
    starts_at_utc INTEGER NOT NULL
        CHECK (typeof(starts_at_utc) = 'integer' AND starts_at_utc >= 0),
    expires_at_utc INTEGER NOT NULL
        CHECK (typeof(expires_at_utc) = 'integer' AND expires_at_utc > starts_at_utc),
    rate_kopecks_per_hour INTEGER NOT NULL
        CHECK (typeof(rate_kopecks_per_hour) = 'integer' AND rate_kopecks_per_hour >= 0),
    amount_kopecks INTEGER NOT NULL
        CHECK (typeof(amount_kopecks) = 'integer' AND amount_kopecks >= 0),
    status TEXT NOT NULL CHECK (status IN ('active', 'completed')),
    plate_snapshot TEXT NOT NULL,
    UNIQUE (id, user_id),
    FOREIGN KEY (vehicle_id, user_id) REFERENCES vehicles(id, user_id)
);

CREATE UNIQUE INDEX sessions_one_active_per_user
    ON sessions(user_id) WHERE status = 'active';
CREATE UNIQUE INDEX sessions_one_active_per_space
    ON sessions(space_id) WHERE status = 'active';
CREATE INDEX sessions_user_history ON sessions(user_id, starts_at_utc);
CREATE INDEX spaces_zone ON spaces(zone_id);

CREATE TABLE payment_operations (
    id TEXT PRIMARY KEY NOT NULL CHECK (length(id) > 0),
    user_id TEXT NOT NULL REFERENCES users(id),
    session_id TEXT NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('book', 'extend')),
    request_json TEXT NOT NULL CHECK (length(request_json) > 0),
    response_json TEXT NOT NULL CHECK (length(response_json) > 0),
    amount_kopecks INTEGER NOT NULL
        CHECK (typeof(amount_kopecks) = 'integer' AND amount_kopecks >= 0),
    created_at_utc INTEGER NOT NULL
        CHECK (typeof(created_at_utc) = 'integer' AND created_at_utc >= 0),
    FOREIGN KEY (session_id, user_id) REFERENCES sessions(id, user_id)
);

CREATE UNIQUE INDEX payment_one_booking_per_session
    ON payment_operations(session_id) WHERE kind = 'book';

PRAGMA user_version = 1;
COMMIT;
