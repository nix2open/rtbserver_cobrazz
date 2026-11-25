-- Add Remaining Even spend_mode ('R') for CCG strategies.
-- Prerequisite: docs/sql/2026-11-20_ccg_spend_mode.sql already applied.
-- 'A' = ASAP, 'E' = Even (from window start, catch-up), 'R' = Remaining Even.

SET lock_timeout = '2s';
SET statement_timeout = '60s';

ALTER TABLE public.campaigncreativegroup
  DROP CONSTRAINT IF EXISTS campaigncreativegroup_spend_mode_chk;

ALTER TABLE public.campaigncreativegroup
  ADD CONSTRAINT campaigncreativegroup_spend_mode_chk
  CHECK (spend_mode IN ('A', 'E', 'R'));
