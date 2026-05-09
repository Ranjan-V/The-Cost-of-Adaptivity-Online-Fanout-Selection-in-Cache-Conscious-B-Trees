#!/usr/bin/env python3
"""Fetch a small real-world Wikipedia pageview trace.

The output is intentionally compact: one row per top article per day. The C++
benchmark expands these rows into a weighted access stream, so the repository
does not need to store a large trace file.
"""

import argparse
import csv
import datetime as dt
import json
import os
import sys
import time
import urllib.error
import urllib.request


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_OUTPUT = os.path.join(ROOT, "data", "wiki_pageviews_trace.csv")
API_TEMPLATE = (
    "https://wikimedia.org/api/rest_v1/metrics/pageviews/top/"
    "en.wikipedia/all-access/{year}/{month:02d}/{day:02d}"
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Fetch recent top-article pageview data from Wikimedia."
    )
    parser.add_argument("--days", type=int, default=7, help="Number of days to fetch.")
    parser.add_argument(
        "--articles-per-day",
        type=int,
        default=500,
        help="Maximum top articles to keep per day.",
    )
    parser.add_argument(
        "--end-date",
        default=None,
        help="Last date to fetch, YYYY-MM-DD. Defaults to two UTC days ago.",
    )
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="Output CSV path.")
    return parser.parse_args()


def request_json(url):
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": (
                "CacheAdaptiveBTreesResearch/0.1 "
                "(local benchmark trace fetcher)"
            )
        },
    )
    delay = 1.0
    for attempt in range(5):
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as exc:
            if exc.code not in (429, 503) or attempt == 4:
                raise
            retry_after = exc.headers.get("Retry-After")
            if retry_after:
                try:
                    delay = max(delay, float(retry_after))
                except ValueError:
                    pass
            print("Rate limited; sleeping {:.1f}s".format(delay))
            time.sleep(delay)
            delay *= 2.0


def fetch_day(day):
    url = API_TEMPLATE.format(year=day.year, month=day.month, day=day.day)
    payload = request_json(url)
    items = payload.get("items", [])
    if not items:
        return []
    return items[0].get("articles", [])


def latest_default_end_date():
    return (dt.datetime.utcnow().date() - dt.timedelta(days=2))


def find_available_end_date(candidate, max_backoff_days):
    day = candidate
    for _ in range(max_backoff_days + 1):
        try:
            articles = fetch_day(day)
            if articles:
                return day
        except (urllib.error.HTTPError, urllib.error.URLError, ValueError):
            pass
        day -= dt.timedelta(days=1)
        time.sleep(0.5)
    raise RuntimeError("Could not find an available Wikimedia top-pageviews day")


def main():
    args = parse_args()
    if args.days <= 0:
        raise SystemExit("--days must be positive")
    if args.articles_per_day <= 0:
        raise SystemExit("--articles-per-day must be positive")

    if args.end_date:
        end_date = dt.datetime.strptime(args.end_date, "%Y-%m-%d").date()
    else:
        end_date = find_available_end_date(latest_default_end_date(), 60)

    start_date = end_date - dt.timedelta(days=args.days - 1)
    output_dir = os.path.dirname(os.path.abspath(args.output))
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    article_ids = {}
    rows = []

    for offset in range(args.days):
        day = start_date + dt.timedelta(days=offset)
        print("Fetching", day.isoformat())
        articles = fetch_day(day)
        kept = 0
        for article in articles:
            title = article.get("article", "")
            if not title or title.startswith("Special:"):
                continue
            if title not in article_ids:
                article_ids[title] = len(article_ids)

            rows.append({
                "day": day.isoformat(),
                "article_id": article_ids[title],
                "views": int(article.get("views", 0)),
                "rank": int(article.get("rank", kept + 1)),
                "article": title.replace(",", " "),
            })
            kept += 1
            if kept >= args.articles_per_day:
                break
        time.sleep(0.2)

    with open(args.output, "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["day", "article_id", "views", "rank", "article"],
        )
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    print("Wrote {} rows and {} unique articles to {}".format(
        len(rows), len(article_ids), args.output
    ))
    return 0


if __name__ == "__main__":
    sys.exit(main())
