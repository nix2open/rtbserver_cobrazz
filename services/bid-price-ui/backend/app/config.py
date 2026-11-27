from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
  model_config = SettingsConfigDict(env_file="config.env", extra="ignore")

  app_name: str = "Bid price"
  bind_host: str = "0.0.0.0"
  bind_port: int = 3091

  clickhouse_host: str = "click00"
  clickhouse_port: int = 8123
  clickhouse_database: str = "default"
  clickhouse_user: str = "default"
  clickhouse_password: str = ""
  clickhouse_table: str = "bid_price_events"
  query_timeout_sec: int = 60
  default_tz: str = "Europe/Moscow"


@lru_cache
def get_settings() -> Settings:
  return Settings()
