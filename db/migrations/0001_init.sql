-- Vertical geofencing sim: one row per scan message, per estimate, per punch. Applied to Supabase
-- project ralco-sim via the Supabase connector (apply_migration); kept here as the source of truth.

create table sessions (
  id           bigint generated always as identity primary key,
  seed         integer      not null,
  started_at   timestamptz  not null default now(),
  source       text         not null check (source in ('live', 'replay')),
  label        text,
  floor_height real         not null,
  num_floors   integer      not null,
  config       jsonb        not null default '{}'::jsonb
);

create table workers (
  session_id   bigint  not null references sessions(id) on delete cascade,
  id           text    not null,
  platform     text    not null,
  route        text,
  target_floor integer,
  primary key (session_id, id)
);

create table beacons (
  session_id   bigint  not null references sessions(id) on delete cascade,
  id           text    not null,
  floor        integer not null,
  x            real    not null,
  y            real    not null,
  z            real    not null,
  tx_power_dbm real    not null default -59,
  primary key (session_id, id)
);

-- What the phone reported. `scans` is the array of {b, rssi} heard; ground truth lives alongside
-- because this is a simulator - a production table would not have the truth_* columns.
create table scans (
  id             bigint generated always as identity primary key,
  session_id     bigint  not null references sessions(id) on delete cascade,
  t              real    not null,
  worker         text    not null,
  platform       text    not null,
  app_state      text    not null,
  phone_state    text    not null,
  scans          jsonb   not null default '[]'::jsonb,
  region_events  jsonb   not null default '[]'::jsonb,
  pressure       real    not null,
  ref_pressure   real    not null,
  truth_floor    integer not null,
  truth_on_hoist boolean not null
);
create index scans_session_t on scans (session_id, t);
create index scans_session_worker_t on scans (session_id, worker, t);

create table estimates (
  id         bigint generated always as identity primary key,
  session_id bigint  not null references sessions(id) on delete cascade,
  t          real    not null,
  worker     text    not null,
  estimator  text    not null,
  floor      integer,
  confidence real    not null,
  state      text    not null check (state in ('stable', 'transit', 'unknown'))
);
create index estimates_session_estimator_t on estimates (session_id, estimator, t);

create table punches (
  id         bigint generated always as identity primary key,
  session_id bigint  not null references sessions(id) on delete cascade,
  t          real    not null,
  worker     text    not null,
  estimator  text    not null,
  kind       text    not null check (kind in ('in', 'out')),
  floor      integer not null,
  verified   boolean not null
);
create index punches_session_t on punches (session_id, t);

create table events (
  id         bigint generated always as identity primary key,
  session_id bigint not null references sessions(id) on delete cascade,
  t          real   not null,
  name       text   not null,
  payload    jsonb  not null default '{}'::jsonb
);
create index events_session_t on events (session_id, t);

-- Latest scorer output per (session, estimator); overwritten as a live session progresses.
create table metrics (
  session_id  bigint      not null references sessions(id) on delete cascade,
  estimator   text        not null,
  computed_at timestamptz not null default now(),
  values      jsonb       not null,
  primary key (session_id, estimator)
);

-- The server writes over a direct Postgres connection. Nothing is exposed through the Supabase
-- REST/anon API: enable RLS with no policies so the public API denies everything.
alter table sessions  enable row level security;
alter table workers   enable row level security;
alter table beacons   enable row level security;
alter table scans     enable row level security;
alter table estimates enable row level security;
alter table punches   enable row level security;
alter table events    enable row level security;
alter table metrics   enable row level security;
