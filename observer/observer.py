"""
observer.py - Main entry point for Tennis Ball Spin Observer.

Connects to the ATOM S3 WebSocket data stream, persists IMU data
and shot events to SQLite, and optionally serves a dashboard.

Usage:
    python observer.py
    python observer.py --ws ws://192.168.4.1:81 --db tennis_data.db
    python observer.py --no-dashboard --port 8080
"""

import asyncio
import argparse
import json
import sys
from datetime import datetime

from db import Database
from spin_analysis import calc_spin_axis


async def ws_receiver(ws_url, db, session_id, state):
    """Connect to WebSocket, receive data, buffer IMU rows, insert shots immediately.

    Implements automatic reconnection with a 2-second retry interval.
    If disconnected for 30+ seconds, a new session is created.

    Parameters
    ----------
    ws_url : str
        WebSocket URL to connect to (e.g. ws://192.168.4.1:81).
    db : Database
        Database instance for data persistence.
    session_id : int
        Current session ID (may be updated in state on reconnect).
    state : dict
        Shared mutable state dictionary for cross-coroutine communication.
    """
    import websockets

    imu_buffer = []
    last_flush = asyncio.get_event_loop().time()
    last_stats_update = last_flush

    while True:
        try:
            async with websockets.connect(ws_url) as ws:
                state['connected'] = True
                state['last_connect'] = datetime.now()

                # Check if we need a new session (disconnected >= 30 seconds)
                if state.get('last_disconnect'):
                    gap = (datetime.now() - state['last_disconnect']).total_seconds()
                    if gap >= 30.0:
                        # Finalize old session
                        avg = state['rpm_sum'] / max(state['total_samples'], 1)
                        db.end_session(
                            state['session_id'], state['total_samples'],
                            state['total_shots'], avg, state['max_rpm']
                        )
                        # Create new session and reset counters
                        new_sid = db.create_session()
                        state['session_id'] = new_sid
                        state['total_samples'] = 0
                        state['total_shots'] = 0
                        state['rpm_sum'] = 0.0
                        state['max_rpm'] = 0.0
                        state['last_shot'] = None

                while True:
                    msg = await ws.recv()
                    data = json.loads(msg)
                    now_str = datetime.now().isoformat(timespec='milliseconds')
                    current_session = state['session_id']

                    if data.get('event') == 'shot':
                        # Shot event -- insert immediately
                        theta, phi = calc_spin_axis(
                            data.get('gx', 0),
                            data.get('gy', 0),
                            data.get('gz', 0),
                        )
                        db.insert_shot(current_session, {
                            'shot_id': data.get('id', 0),
                            'device_ts': data.get('t', 0),
                            'rpm': data.get('rpm', 0),
                            'peak_g': data.get('peakG', 0),
                            'gx': data.get('gx', 0),
                            'gy': data.get('gy', 0),
                            'gz': data.get('gz', 0),
                            'spin_type': data.get('type', ''),
                            'spin_axis_theta': theta,
                            'spin_axis_phi': phi,
                        })
                        state['total_shots'] += 1
                        state['last_shot'] = data
                    else:
                        # Regular IMU data frame
                        rpm_val = data.get('rpm', 0)
                        state['total_samples'] += 1
                        state['rpm_sum'] += rpm_val
                        if rpm_val > state['max_rpm']:
                            state['max_rpm'] = rpm_val

                        imu_buffer.append({
                            'session_id': current_session,
                            'device_ts': data.get('t', 0),
                            'local_ts': now_str,
                            'ax': data.get('ax', 0),
                            'ay': data.get('ay', 0),
                            'az': data.get('az', 0),
                            'gx': data.get('gx', 0),
                            'gy': data.get('gy', 0),
                            'gz': data.get('gz', 0),
                            'qw': data.get('qw', 0),
                            'qx': data.get('qx', 0),
                            'qy': data.get('qy', 0),
                            'qz': data.get('qz', 0),
                            'rpm': rpm_val,
                            'spin': data.get('spin', ''),
                            'impact': data.get('imp', 0),
                        })

                    # Flush buffer every 100 rows or every 2 seconds
                    now_time = asyncio.get_event_loop().time()
                    if len(imu_buffer) >= 100 or (now_time - last_flush) >= 2.0:
                        if imu_buffer:
                            db.insert_imu_batch(imu_buffer)
                            imu_buffer.clear()
                            last_flush = now_time

                    # Update session stats every 5 seconds
                    if (now_time - last_stats_update) >= 5.0:
                        avg = state['rpm_sum'] / max(state['total_samples'], 1)
                        db.end_session(
                            current_session, state['total_samples'],
                            state['total_shots'], avg, state['max_rpm']
                        )
                        last_stats_update = now_time

        except Exception as e:
            state['connected'] = False
            state['last_disconnect'] = datetime.now()
            # Flush any remaining buffered data before waiting
            if imu_buffer:
                try:
                    db.insert_imu_batch(imu_buffer)
                    imu_buffer.clear()
                except Exception:
                    pass
            await asyncio.sleep(2)


async def terminal_display(state, db_path, ws_url, dashboard_url):
    """Print live status to the terminal, refreshing every second.

    Parameters
    ----------
    state : dict
        Shared state dictionary with connection and stats info.
    db_path : str
        Path to the database file (for display).
    ws_url : str
        WebSocket URL (for display).
    dashboard_url : str
        Dashboard URL (for display).
    """
    while True:
        session_id = state['session_id']
        avg_rpm = (
            state['rpm_sum'] / state['total_samples']
            if state['total_samples'] > 0
            else 0
        )
        connected = state['connected']
        status = "Connected" if connected else "Disconnected"
        indicator = "\u25cf" if connected else "\u25cb"

        # Clear screen and move cursor to top-left
        print("\033[2J\033[H", end="")
        print("Tennis Ball Observer v1.0")
        print("=" * 40)
        print(f"WebSocket:  {ws_url}  {indicator} {status}")
        print(f"Database:   {db_path}")
        print(f"Dashboard:  {dashboard_url}")
        print(f"Session:    #{session_id}")
        print("=" * 40)
        print(f"Samples:    {state['total_samples']}")
        print(f"Shots:      {state['total_shots']}")
        if state['last_shot']:
            ls = state['last_shot']
            print(
                f"Last shot:  {ls.get('type', '')}  "
                f"{ls.get('rpm', 0):.0f} RPM  "
                f"{ls.get('peakG', 0):.1f}g"
            )
        print(f"Avg RPM:    {avg_rpm:.0f}")
        print(f"Max RPM:    {state['max_rpm']:.0f}")
        print("=" * 40)
        print("Press Ctrl+C to stop")

        await asyncio.sleep(1)


async def main():
    """Parse arguments, initialise database and session, launch all tasks."""
    parser = argparse.ArgumentParser(description='Tennis Ball Spin Observer')
    parser.add_argument(
        '--ws', default='ws://192.168.4.1:81',
        help='WebSocket URL (default: ws://192.168.4.1:81)',
    )
    parser.add_argument(
        '--db', default='tennis_data.db',
        help='Database file path (default: tennis_data.db)',
    )
    parser.add_argument(
        '--no-dashboard', action='store_true',
        help='Disable the web dashboard',
    )
    parser.add_argument(
        '--port', type=int, default=8080,
        help='Dashboard HTTP port (default: 8080)',
    )
    args = parser.parse_args()

    db = Database(args.db)
    session_id = db.create_session()

    state = {
        'connected': False,
        'session_id': session_id,
        'total_samples': 0,
        'total_shots': 0,
        'rpm_sum': 0.0,
        'max_rpm': 0.0,
        'last_shot': None,
        'last_connect': None,
        'last_disconnect': None,
    }

    dashboard_url = f"http://localhost:{args.port}"

    tasks = [
        asyncio.create_task(ws_receiver(args.ws, db, session_id, state)),
        asyncio.create_task(terminal_display(state, args.db, args.ws, dashboard_url)),
    ]

    if not args.no_dashboard:
        from dashboard import create_app
        from aiohttp import web

        app = create_app(db)
        runner = web.AppRunner(app)
        await runner.setup()
        site = web.TCPSite(runner, '0.0.0.0', args.port)
        await site.start()
        print(f"Dashboard: {dashboard_url}")

    try:
        await asyncio.gather(*tasks)
    except KeyboardInterrupt:
        pass
    finally:
        # Flush final session statistics
        avg = state['rpm_sum'] / max(state['total_samples'], 1)
        db.end_session(
            state['session_id'], state['total_samples'],
            state['total_shots'], avg, state['max_rpm']
        )
        db.close()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nStopped.")
