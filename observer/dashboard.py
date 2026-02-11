"""
dashboard.py - aiohttp web application for the Tennis Ball Spin Observer.

Serves the single-page dashboard HTML and provides REST API endpoints
for session management, shot data retrieval, and CSV export.
"""

import os
import json

from aiohttp import web

STATIC_DIR = os.path.join(os.path.dirname(__file__), 'static')


def create_app(db):
    """Create and configure the aiohttp web application.

    Parameters
    ----------
    db : Database
        An instance of the Database class from db.py.

    Returns
    -------
    web.Application
        The configured aiohttp application.
    """
    app = web.Application()
    app['db'] = db

    # --- Route handlers ---

    async def index(request):
        """Serve the main dashboard HTML page."""
        return web.FileResponse(os.path.join(STATIC_DIR, 'dashboard.html'))

    async def api_sessions(request):
        """Return a JSON list of all sessions, most recent first."""
        sessions = request.app['db'].get_sessions()
        return web.json_response(sessions)

    async def api_session(request):
        """Return JSON detail for a single session by ID."""
        sid = int(request.match_info['id'])
        session = request.app['db'].get_session(sid)
        if not session:
            return web.json_response({'error': 'not found'}, status=404)
        return web.json_response(session)

    async def api_shots(request):
        """Return all shots for a given session ID."""
        sid = int(request.match_info['id'])
        shots = request.app['db'].get_shots(sid)
        return web.json_response(shots)

    async def api_imu_data(request):
        """Return IMU data for a session with optional limit query param."""
        sid = int(request.match_info['id'])
        limit = int(request.query.get('limit', 5000))
        data = request.app['db'].get_imu_data(sid, limit)
        return web.json_response(data)

    async def api_delete_session(request):
        """Delete a session and all associated data."""
        sid = int(request.match_info['id'])
        request.app['db'].delete_session(sid)
        return web.json_response({'ok': True})

    async def api_export_csv(request):
        """Export a session as a downloadable CSV file."""
        sid = int(request.match_info['id'])
        csv_data = request.app['db'].export_csv(sid)
        return web.Response(
            text=csv_data,
            content_type='text/csv',
            headers={
                'Content-Disposition': f'attachment; filename=session_{sid}.csv'
            }
        )

    # --- Register routes ---

    app.router.add_get('/', index)
    app.router.add_get('/api/sessions', api_sessions)
    app.router.add_get('/api/sessions/{id}', api_session)
    app.router.add_get('/api/sessions/{id}/shots', api_shots)
    app.router.add_get('/api/sessions/{id}/imu', api_imu_data)
    app.router.add_delete('/api/sessions/{id}', api_delete_session)
    app.router.add_get('/api/sessions/{id}/export', api_export_csv)

    return app
