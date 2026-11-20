-- Even / ASAP spend pacing for CCG (strategy level).
-- Safe for PostgreSQL 9.4 (no ADD ... NOT NULL DEFAULT rewrite).
-- Target: stat.public.campaigncreativegroup

SET lock_timeout = '2s';
SET statement_timeout = '180s';

ALTER TABLE public.campaigncreativegroup
  ADD COLUMN spend_mode character(1);

ALTER TABLE public.campaigncreativegroup
  ALTER COLUMN spend_mode SET DEFAULT 'A';

UPDATE public.campaigncreativegroup
SET spend_mode = 'A'
WHERE spend_mode IS NULL;

ALTER TABLE public.campaigncreativegroup
  ALTER COLUMN spend_mode SET NOT NULL;

ALTER TABLE public.campaigncreativegroup
  ADD CONSTRAINT campaigncreativegroup_spend_mode_chk
  CHECK (spend_mode IN ('A', 'E'));

-- 'A' = ASAP (default), 'E' = Even
