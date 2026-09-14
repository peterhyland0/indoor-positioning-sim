# Database

Hosted Postgres on Supabase: project **ralco-sim** (`oijgcusxitphiuwakruz`, eu-west-1, free tier),
organisation "peterhyland0's Org". Schema in `migrations/`, applied through the Supabase connector
(`apply_migration`); `0001_init.sql` is live.

Tables: `sessions`, `workers`, `beacons`, `scans` (one row per scan message, beacons heard as jsonb),
`estimates` (one row per estimator per scan), `punches`, `events`, `metrics` (latest scorer output per
session × estimator). All tables have RLS enabled with no policies, so the public Supabase API denies
everything; only the server's direct connection reads and writes.

## Connection string for the server

The connector cannot read the database password. Get the connection string once from the dashboard:
Supabase → project `ralco-sim` → **Connect** → *Session pooler* (IPv4-friendly) → copy the URI, and put it in
`apps/server/.env` as `DATABASE_URL=postgresql://postgres.oijgcusxitphiuwakruz:<password>@aws-1-eu-west-1.pooler.supabase.com:5432/postgres`.
Reset the password under Project Settings → Database if you don't have it. `.env` is gitignored.
Without `DATABASE_URL` the server still runs; persistence is simply off.
