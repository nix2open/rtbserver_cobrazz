-- Hide referer on click redirect (CCG / strategy level).
-- Safe for PostgreSQL 9.4 (no ADD ... NOT NULL DEFAULT rewrite).
-- Target: public.campaigncreativegroup
-- Default false on first rollout: existing behaviour unchanged.

SET lock_timeout = '2s';
SET statement_timeout = '180s';

ALTER TABLE public.campaigncreativegroup
  ADD COLUMN hide_referer boolean;

ALTER TABLE public.campaigncreativegroup
  ALTER COLUMN hide_referer SET DEFAULT false;

UPDATE public.campaigncreativegroup
SET hide_referer = false
WHERE hide_referer IS NULL;

ALTER TABLE public.campaigncreativegroup
  ALTER COLUMN hide_referer SET NOT NULL;
