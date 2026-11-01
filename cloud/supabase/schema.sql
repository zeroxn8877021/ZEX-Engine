-- ZEX INTERACTIVE cloud schema for Supabase (PostgreSQL + Row Level Security).
-- Run this once in the Supabase SQL editor. Never put the service_role key in the engine or the admin page.

create extension if not exists pgcrypto;

-- ---------- profiles ----------
create table if not exists public.profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  email text,
  display_name text,
  role text not null default 'user' check (role in ('user','admin','super_admin')),
  engine_version text,
  last_seen_at timestamptz,
  created_at timestamptz not null default now()
);

create or replace function public.is_admin() returns boolean
language sql stable security definer set search_path = public as $$
  select exists (select 1 from public.profiles where id = auth.uid() and role in ('admin','super_admin'));
$$;

create or replace function public.is_super_admin() returns boolean
language sql stable security definer set search_path = public as $$
  select exists (select 1 from public.profiles where id = auth.uid() and role = 'super_admin');
$$;

create or replace function public.handle_new_user() returns trigger
language plpgsql security definer set search_path = public as $$
begin
  insert into public.profiles (id, email) values (new.id, new.email) on conflict do nothing;
  return new;
end $$;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created after insert on auth.users
  for each row execute function public.handle_new_user();

-- Users can never change their own role; only a super_admin can change roles.
create or replace function public.protect_profile_role() returns trigger
language plpgsql security definer set search_path = public as $$
begin
  if new.role is distinct from old.role and not public.is_super_admin() then
    raise exception 'Only a super_admin can change roles';
  end if;
  return new;
end $$;

drop trigger if exists protect_role on public.profiles;
create trigger protect_role before update on public.profiles
  for each row execute function public.protect_profile_role();

alter table public.profiles enable row level security;
drop policy if exists "profiles read own" on public.profiles;
drop policy if exists "profiles read admin" on public.profiles;
drop policy if exists "profiles update own" on public.profiles;
drop policy if exists "profiles update super admin" on public.profiles;
create policy "profiles read own" on public.profiles for select using (id = auth.uid());
create policy "profiles read admin" on public.profiles for select using (public.is_admin());
create policy "profiles update own" on public.profiles for update using (id = auth.uid()) with check (id = auth.uid());
create policy "profiles update super admin" on public.profiles for update using (public.is_super_admin());

-- ---------- engine releases (update channels, signed manifests) ----------
create table if not exists public.engine_releases (
  id uuid primary key default gen_random_uuid(),
  version text not null,
  channel text not null default 'stable' check (channel in ('stable','beta','dev')),
  release_notes text,
  min_supported_version text,
  download_url text,
  sha256 text,
  manifest_signature text,
  published boolean not null default false,
  created_by uuid references auth.users(id),
  created_at timestamptz not null default now(),
  unique (version, channel)
);
alter table public.engine_releases enable row level security;
drop policy if exists "releases public read" on public.engine_releases;
drop policy if exists "releases admin all" on public.engine_releases;
create policy "releases public read" on public.engine_releases for select using (published or public.is_admin());
create policy "releases admin all" on public.engine_releases for all using (public.is_admin()) with check (public.is_admin());

-- ---------- remote config + feature flags ----------
create table if not exists public.remote_config (
  key text primary key,
  value jsonb not null,
  is_feature_flag boolean not null default false,
  updated_by uuid references auth.users(id),
  updated_at timestamptz not null default now()
);
alter table public.remote_config enable row level security;
drop policy if exists "config public read" on public.remote_config;
drop policy if exists "config admin all" on public.remote_config;
create policy "config public read" on public.remote_config for select using (true);
create policy "config admin all" on public.remote_config for all using (public.is_admin()) with check (public.is_admin());

-- ---------- announcements ----------
create table if not exists public.announcements (
  id uuid primary key default gen_random_uuid(),
  title text not null,
  body text,
  kind text not null default 'info' check (kind in ('info','maintenance','update')),
  active boolean not null default true,
  created_at timestamptz not null default now()
);
alter table public.announcements enable row level security;
drop policy if exists "announcements public read" on public.announcements;
drop policy if exists "announcements admin all" on public.announcements;
create policy "announcements public read" on public.announcements for select using (active or public.is_admin());
create policy "announcements admin all" on public.announcements for all using (public.is_admin()) with check (public.is_admin());

-- ---------- cloud projects (metadata only; no source code is uploaded unless the user opts in) ----------
create table if not exists public.projects (
  id uuid primary key default gen_random_uuid(),
  owner uuid not null references auth.users(id) on delete cascade default auth.uid(),
  name text not null,
  engine_version text,
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);
alter table public.projects enable row level security;
drop policy if exists "projects owner all" on public.projects;
drop policy if exists "projects admin read" on public.projects;
create policy "projects owner all" on public.projects for all using (owner = auth.uid()) with check (owner = auth.uid());
create policy "projects admin read" on public.projects for select using (public.is_admin());

-- ---------- asset library metadata ----------
create table if not exists public.assets (
  id uuid primary key default gen_random_uuid(),
  name text not null,
  category text,
  version text not null default '1.0.0',
  storage_path text,
  sha256 text,
  published boolean not null default false,
  created_at timestamptz not null default now()
);
alter table public.assets enable row level security;
drop policy if exists "assets public read" on public.assets;
drop policy if exists "assets admin all" on public.assets;
create policy "assets public read" on public.assets for select using (published or public.is_admin());
create policy "assets admin all" on public.assets for all using (public.is_admin()) with check (public.is_admin());

-- ---------- audit log (admins can read and append; nobody can edit or delete) ----------
create table if not exists public.audit_log (
  id bigint generated always as identity primary key,
  actor uuid references auth.users(id) default auth.uid(),
  action text not null,
  target text,
  details jsonb,
  created_at timestamptz not null default now()
);
alter table public.audit_log enable row level security;
drop policy if exists "audit admin read" on public.audit_log;
drop policy if exists "audit admin insert" on public.audit_log;
create policy "audit admin read" on public.audit_log for select using (public.is_admin());
create policy "audit admin insert" on public.audit_log for insert with check (public.is_admin() and actor = auth.uid());

-- Trigger functions must not be callable through the API.
revoke execute on function public.handle_new_user() from public, anon, authenticated;
revoke execute on function public.protect_profile_role() from public, anon, authenticated;

-- After creating your own account, promote it ONCE from the SQL editor (this runs with full rights):
--   update public.profiles set role = 'super_admin' where email = 'you@example.com';
