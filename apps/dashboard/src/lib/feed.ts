'use client';
// Live feed: WebSocket to the server's /ui path with reconnect; frames go through the reducer.
import { useEffect, useReducer, useRef } from 'react';
import { EMPTY_STATE, reduce, type DashboardState, type UiFrame } from './state';

export const SERVER_URL = process.env['NEXT_PUBLIC_SERVER_URL'] ?? 'http://localhost:8080';
export const WS_URL = SERVER_URL.replace(/^http/, 'ws') + '/ui';

type Action = UiFrame | { type: 'disconnected' };

function reducer(s: DashboardState, a: Action): DashboardState {
  if (a.type === 'disconnected') return { ...s, connected: false, unrealConnected: false };
  return reduce(s, a);
}

export function useFeed(): { state: DashboardState; send: (cmd: object) => void } {
  const [state, dispatch] = useReducer(reducer, EMPTY_STATE);
  const wsRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    let closed = false;
    let ws: WebSocket | null = null;
    let retry = 1000;
    const connect = () => {
      if (closed) return;
      ws = new WebSocket(WS_URL);
      wsRef.current = ws;
      ws.onmessage = (ev) => dispatch(JSON.parse(ev.data as string) as UiFrame);
      ws.onopen = () => { retry = 1000; };
      ws.onclose = () => { dispatch({ type: 'disconnected' }); if (!closed) setTimeout(connect, retry); retry = Math.min(retry * 2, 10000); };
      ws.onerror = () => ws?.close();
    };
    connect();
    return () => { closed = true; ws?.close(); };
  }, []);

  return { state, send: (cmd) => { if (wsRef.current?.readyState === WebSocket.OPEN) wsRef.current.send(JSON.stringify(cmd)); } };
}

export async function postCommand(cmd: string): Promise<boolean> {
  const r = await fetch(`${SERVER_URL}/api/command`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ cmd }) });
  return r.ok && ((await r.json()) as { sent: boolean }).sent;
}
