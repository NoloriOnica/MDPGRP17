# Robot Path Finding Simulator

## Prerequisites

- macOS
- Node.js 18+

## Run in Browser (Development)

1. Install dependencies:
   `npm install`
2. Set `GEMINI_API_KEY` in `.env.local` if needed by your features.
3. Start dev server:
   `npm run dev`

## Run as Desktop App (Development)

1. Start the Vite dev server:
   `npm run dev`
2. In a second terminal, start Electron:
   `npm run desktop`

## Build a Native macOS App (`.app`)

1. Build and package:
   `npm run build:mac-app`
2. Your app bundle will be created at:
   `release/Robot Path Finding Simulator.app`

If macOS blocks first launch, right-click the app and click **Open** once.

## API Integration (Live Sync + Robot Dispatch)

The app now supports a full API bridge in the sidebar under `API Bridge`.

1. Set `Input API URL`:
   - This endpoint should return obstacle data as either:
   - `[{ "id": 1, "x": 50, "y": 50, "d": "E" }]`
   - or `{ "data": [...] }`
   - or `{ "obstacles": [...] }`
2. Click `SYNC + PLAN`:
   - The app fetches obstacle positions from API.
   - The simulator updates immediately and recalculates Dubins path.
3. Confirm dispatch prompt (if `Prompt Send` is enabled):
   - The app sends instructions to `Output API URL`.
   - Payload includes both string instructions and structured commands.

Example output payload sent to robot API:

```json
{
  "obstacles": [
    { "id": 1, "x": 50, "y": 50, "d": "E" }
  ],
  "instructions": ["FORWARD 10", "RIGHT 50", "LEFT 40"],
  "commands": [
    { "type": "FORWARD", "value": 10 },
    { "type": "RIGHT", "value": 50 },
    { "type": "LEFT", "value": 40 }
  ],
  "generatedAt": "2026-02-20T00:00:00.000Z"
}
```

Optional:
- Enable `Live Sync` to poll input API continuously.
- Set `Live Sync Interval (sec)` to control polling frequency.

## Raspberry Pi Pull Model

If the robot (Raspberry Pi) should fetch instructions on demand, call:

`GET /instructions`

This endpoint now performs `sync + plan` on request (using latest `/obstacles`) and returns fresh instructions in the same response.

Example:

```bash
curl -s "http://<SERVER_IP>:8000/instructions"
```

Optional query flags:
- `sync_plan=true` (default): recompute before returning.
- `sync_plan=false`: return the last cached instruction list only.
