"""
db.py - SQLite database wrapper for Tennis Ball Spin Observer.

Handles session management, IMU data storage, shot events,
and data export functionality.
"""

import csv
import io
import sqlite3
from datetime import datetime


class Database:
    """SQLite database wrapper for tennis training data."""

    def __init__(self, db_path="tennis_data.db"):
        """Open database connection and ensure tables exist.

        Parameters
        ----------
        db_path : str
            Path to the SQLite database file.
        """
        self.db_path = db_path
        self.conn = sqlite3.connect(db_path)
        self.conn.row_factory = sqlite3.Row
        self.conn.execute("PRAGMA journal_mode=WAL")
        self._create_tables()

    def _create_tables(self):
        """Create tables and indexes if they do not already exist."""
        self.conn.executescript("""
            CREATE TABLE IF NOT EXISTS sessions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                start_time TEXT,
                end_time TEXT,
                duration_sec REAL,
                total_samples INTEGER DEFAULT 0,
                total_shots INTEGER DEFAULT 0,
                avg_rpm REAL DEFAULT 0,
                max_rpm REAL DEFAULT 0,
                notes TEXT
            );

            CREATE TABLE IF NOT EXISTS imu_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id INTEGER,
                device_ts INTEGER,
                local_ts TEXT,
                ax REAL,
                ay REAL,
                az REAL,
                gx REAL,
                gy REAL,
                gz REAL,
                qw REAL,
                qx REAL,
                qy REAL,
                qz REAL,
                rpm REAL,
                spin TEXT,
                impact INTEGER
            );

            CREATE INDEX IF NOT EXISTS idx_imu_session_ts
                ON imu_data (session_id, device_ts);

            CREATE TABLE IF NOT EXISTS shots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id INTEGER,
                shot_id INTEGER,
                device_ts INTEGER,
                local_ts TEXT,
                rpm REAL,
                peak_g REAL,
                gx REAL,
                gy REAL,
                gz REAL,
                spin_type TEXT,
                spin_axis_theta REAL,
                spin_axis_phi REAL
            );
        """)
        self.conn.commit()

    def create_session(self) -> int:
        """Create a new training session.

        Returns
        -------
        int
            The ID of the newly created session.
        """
        now = datetime.now().isoformat(timespec='milliseconds')
        cursor = self.conn.execute(
            "INSERT INTO sessions (start_time) VALUES (?)",
            (now,)
        )
        self.conn.commit()
        return cursor.lastrowid

    def end_session(self, session_id, total_samples, total_shots, avg_rpm, max_rpm):
        """Update session with final statistics and end time.

        Parameters
        ----------
        session_id : int
            The session to update.
        total_samples : int
            Total number of IMU data samples recorded.
        total_shots : int
            Total number of shots detected.
        avg_rpm : float
            Average RPM across all samples.
        max_rpm : float
            Maximum RPM observed.
        """
        now = datetime.now().isoformat(timespec='milliseconds')
        # Retrieve start_time to compute duration
        row = self.conn.execute(
            "SELECT start_time FROM sessions WHERE id = ?",
            (session_id,)
        ).fetchone()
        duration_sec = 0.0
        if row and row['start_time']:
            try:
                start_dt = datetime.fromisoformat(row['start_time'])
                end_dt = datetime.fromisoformat(now)
                duration_sec = (end_dt - start_dt).total_seconds()
            except (ValueError, TypeError):
                pass
        self.conn.execute(
            """UPDATE sessions
               SET end_time = ?, duration_sec = ?,
                   total_samples = ?, total_shots = ?,
                   avg_rpm = ?, max_rpm = ?
             WHERE id = ?""",
            (now, duration_sec, total_samples, total_shots, avg_rpm, max_rpm, session_id)
        )
        self.conn.commit()

    def insert_imu_batch(self, rows: list):
        """Insert a batch of IMU data rows for performance.

        Parameters
        ----------
        rows : list of dict
            Each dict has keys matching the imu_data columns:
            session_id, device_ts, local_ts, ax, ay, az,
            gx, gy, gz, qw, qx, qy, qz, rpm, spin, impact.
        """
        if not rows:
            return
        self.conn.executemany(
            """INSERT INTO imu_data
               (session_id, device_ts, local_ts,
                ax, ay, az, gx, gy, gz,
                qw, qx, qy, qz, rpm, spin, impact)
             VALUES
               (:session_id, :device_ts, :local_ts,
                :ax, :ay, :az, :gx, :gy, :gz,
                :qw, :qx, :qy, :qz, :rpm, :spin, :impact)""",
            rows
        )
        self.conn.commit()

    def insert_shot(self, session_id, shot_data: dict):
        """Insert a single shot event and commit immediately.

        Parameters
        ----------
        session_id : int
            The session this shot belongs to.
        shot_data : dict
            Keys: shot_id, device_ts, rpm, peak_g, gx, gy, gz,
            spin_type, spin_axis_theta, spin_axis_phi.
        """
        now = datetime.now().isoformat(timespec='milliseconds')
        self.conn.execute(
            """INSERT INTO shots
               (session_id, shot_id, device_ts, local_ts,
                rpm, peak_g, gx, gy, gz,
                spin_type, spin_axis_theta, spin_axis_phi)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (
                session_id,
                shot_data.get('shot_id', 0),
                shot_data.get('device_ts', 0),
                now,
                shot_data.get('rpm', 0),
                shot_data.get('peak_g', 0),
                shot_data.get('gx', 0),
                shot_data.get('gy', 0),
                shot_data.get('gz', 0),
                shot_data.get('spin_type', ''),
                shot_data.get('spin_axis_theta'),
                shot_data.get('spin_axis_phi'),
            )
        )
        self.conn.commit()

    def get_sessions(self) -> list:
        """Retrieve all sessions ordered by most recent first.

        Returns
        -------
        list of dict
            All sessions as dictionaries.
        """
        rows = self.conn.execute(
            "SELECT * FROM sessions ORDER BY id DESC"
        ).fetchall()
        return [dict(row) for row in rows]

    def get_session(self, session_id) -> dict:
        """Retrieve a single session by ID.

        Parameters
        ----------
        session_id : int
            The session ID to look up.

        Returns
        -------
        dict or None
            The session as a dictionary, or None if not found.
        """
        row = self.conn.execute(
            "SELECT * FROM sessions WHERE id = ?",
            (session_id,)
        ).fetchone()
        return dict(row) if row else None

    def get_shots(self, session_id) -> list:
        """Retrieve all shots for a session, ordered by device timestamp.

        Parameters
        ----------
        session_id : int
            The session to query.

        Returns
        -------
        list of dict
            All shots as dictionaries.
        """
        rows = self.conn.execute(
            "SELECT * FROM shots WHERE session_id = ? ORDER BY device_ts",
            (session_id,)
        ).fetchall()
        return [dict(row) for row in rows]

    def get_imu_data(self, session_id, limit=5000) -> list:
        """Retrieve IMU data for a session with a row limit.

        Parameters
        ----------
        session_id : int
            The session to query.
        limit : int
            Maximum number of rows to return (default 5000).

        Returns
        -------
        list of dict
            IMU data rows as dictionaries.
        """
        rows = self.conn.execute(
            "SELECT * FROM imu_data WHERE session_id = ? ORDER BY device_ts LIMIT ?",
            (session_id, limit)
        ).fetchall()
        return [dict(row) for row in rows]

    def delete_session(self, session_id):
        """Delete a session and all associated IMU data and shots.

        Parameters
        ----------
        session_id : int
            The session to delete.
        """
        self.conn.execute(
            "DELETE FROM imu_data WHERE session_id = ?",
            (session_id,)
        )
        self.conn.execute(
            "DELETE FROM shots WHERE session_id = ?",
            (session_id,)
        )
        self.conn.execute(
            "DELETE FROM sessions WHERE id = ?",
            (session_id,)
        )
        self.conn.commit()

    def export_csv(self, session_id) -> str:
        """Generate a CSV string containing IMU data and shots for a session.

        The output has two sections separated by a blank line:
        1. IMU Data section with all imu_data columns
        2. Shots section with all shots columns

        Parameters
        ----------
        session_id : int
            The session to export.

        Returns
        -------
        str
            CSV-formatted string.
        """
        output = io.StringIO()
        writer = csv.writer(output)

        # IMU Data section
        writer.writerow(["# IMU Data"])
        imu_rows = self.get_imu_data(session_id, limit=999999)
        if imu_rows:
            headers = list(imu_rows[0].keys())
            writer.writerow(headers)
            for row in imu_rows:
                writer.writerow([row[h] for h in headers])

        writer.writerow([])

        # Shots section
        writer.writerow(["# Shots"])
        shot_rows = self.get_shots(session_id)
        if shot_rows:
            headers = list(shot_rows[0].keys())
            writer.writerow(headers)
            for row in shot_rows:
                writer.writerow([row[h] for h in headers])

        return output.getvalue()

    def close(self):
        """Close the database connection."""
        if self.conn:
            self.conn.close()
            self.conn = None
