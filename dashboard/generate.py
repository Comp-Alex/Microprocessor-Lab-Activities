#!/usr/bin/env python3
"""Build a static microprocessor engineering dashboard."""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE_PATH = ROOT / "dashboard" / "templates" / "index.html"
SOURCE_EXTENSIONS = {".asm", ".s", ".S", ".c", ".cc", ".cpp", ".h", ".hpp", ".ino"}
EXCLUDED_DIRS = {".git", ".venv", "build", "dist", "node_modules", "__pycache__"}
TRACKS = (
    ("assembly", "Assembly programming"),
    ("labs", "Lab experiments"),
    ("architecture", "Architecture concepts"),
    ("hardware", "Hardware activities"),
)


def discover_repository() -> str | None:
    """Return the owner/repository from the environment or origin remote."""
    configured = os.environ.get("GITHUB_REPOSITORY")
    if configured:
        return configured

    try:
        remote = subprocess.run(
            ["git", "config", "--get", "remote.origin.url"],
            cwd=ROOT,
            capture_output=True,
            check=True,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None

    match = re.search(r"github\.com[:/]([^/]+/[^/.]+?)(?:\.git)?$", remote)
    return match.group(1) if match else None


def classify_source(path: Path, content: str) -> str:
    """Assign a source artifact to one track using its name and contents."""
    name = path.stem.lower()
    if path.suffix in {".asm", ".s", ".S"} or "assembly" in name:
        return "assembly"
    if re.search(r"\b(instruction set|register file|datapath|cpu architecture)\b", content, re.I):
        return "architecture"
    if re.match(r"^(exp|experiment|lab)[-_ ]?\d+", name):
        return "labs"
    return "hardware"


def collect_tracks(root: Path = ROOT) -> tuple[dict[str, int], int]:
    """Count tracked source artifacts by engineering track."""
    counts = {key: 0 for key, _ in TRACKS}
    total = 0
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in SOURCE_EXTENSIONS:
            continue
        relative = path.relative_to(root)
        if any(part in EXCLUDED_DIRS for part in relative.parts):
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        counts[classify_source(path, content)] += 1
        total += 1
    return counts, total


def github_json(repository: str, resource: str, token: str | None) -> object:
    url = f"https://api.github.com/repos/{quote(repository, safe='/')}/{resource}"
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "microprocessor-engineering-dashboard",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = Request(url, headers=headers)
    with urlopen(request, timeout=15) as response:
        return json.loads(response.read().decode("utf-8"))


def collect_github_data(repository: str | None, token: str | None) -> dict[str, object]:
    if not repository:
        return {
            "repository_url": "",
            "stats": None,
            "commits": [],
            "milestones": [],
            "error": "Set GITHUB_REPOSITORY or pass --repository to load live GitHub data.",
        }

    try:
        repo = github_json(repository, "", token)
        commits = github_json(repository, "commits?per_page=5", token)
        milestones = github_json(repository, "milestones?state=all&per_page=100", token)
    except (HTTPError, URLError, TimeoutError, json.JSONDecodeError) as error:
        detail = getattr(error, "reason", None) or getattr(error, "code", None) or str(error)
        return {
            "repository_url": f"https://github.com/{repository}",
            "stats": None,
            "commits": [],
            "milestones": [],
            "error": f"GitHub data could not be loaded ({detail}). Check network access and token permissions.",
        }

    if not isinstance(repo, dict) or not isinstance(commits, list) or not isinstance(milestones, list):
        raise ValueError("GitHub API returned an unexpected response shape.")

    return {
        "repository_url": repo.get("html_url", f"https://github.com/{repository}"),
        "stats": {
            "stars": repo.get("stargazers_count", 0),
            "forks": repo.get("forks_count", 0),
            "issues": repo.get("open_issues_count", 0),
            "branch": repo.get("default_branch", "main"),
        },
        "commits": commits,
        "milestones": milestones,
        "error": "",
    }


def format_stats(stats: object, total_sources: int) -> str:
    entries = [
        ("Source files", total_sources),
        ("Stars", stats.get("stars", 0) if isinstance(stats, dict) else None),
        ("Forks", stats.get("forks", 0) if isinstance(stats, dict) else None),
        ("Open issues", stats.get("issues", 0) if isinstance(stats, dict) else None),
    ]
    return "\n".join(
        f'<article class="stat-card"><span>{html.escape(label)}</span>'
        f'<strong>{html.escape(str(value)) if value is not None else "—"}</strong></article>'
        for label, value in entries
    )


def format_tracks(counts: dict[str, int], total: int) -> str:
    cards = []
    for key, label in TRACKS:
        count = counts[key]
        share = round(count * 100 / total) if total else 0
        cards.append(
            '<article class="track-card">'
            '<div class="track-heading"><h3>{}</h3><span>{} / {} files</span></div>'
            '<div class="bar-row"><progress max="100" value="{}" aria-label="{}: {} percent of tracked source files"></progress>'
            '<strong>{}%</strong></div></article>'.format(
                html.escape(label), count, total, share, html.escape(label), share, share
            )
        )
    return "\n".join(cards)


def format_milestones(milestones: object) -> str:
    if not isinstance(milestones, list):
        return ""
    cards = []
    for milestone in milestones:
        if not isinstance(milestone, dict):
            continue
        closed = milestone.get("closed_issues", 0)
        open_issues = milestone.get("open_issues", 0)
        total_issues = closed + open_issues
        progress = round(closed * 100 / total_issues) if total_issues else 0
        due = milestone.get("due_on")
        due_label = f"Due {due[:10]}" if isinstance(due, str) else "No due date"
        title = html.escape(str(milestone.get("title", "Untitled milestone")))
        url = html.escape(str(milestone.get("html_url", "#")), quote=True)
        state = html.escape(str(milestone.get("state", "open")).title())
        cards.append(
            '<article class="milestone-card"><div class="milestone-heading">'
            f'<a href="{url}">{title}</a><span class="pill">{state}</span></div>'
            f'<p>{closed} of {total_issues} linked issues closed · {html.escape(due_label)}</p>'
            f'<progress max="100" value="{progress}" aria-label="{title}: {progress} percent complete"></progress>'
            f'<span class="milestone-percent">{progress}%</span></article>'
        )
    if not cards:
        return '<p class="empty-state">No GitHub milestones configured yet. Add milestones to your repository to track lab checkpoints here.</p>'
    return "\n".join(cards)


def format_commits(commits: object) -> str:
    if not isinstance(commits, list):
        return ""
    cards = []
    for commit in commits:
        if not isinstance(commit, dict):
            continue
        details = commit.get("commit", {})
        if not isinstance(details, dict):
            continue
        author = details.get("author", {})
        message = str(details.get("message", "Commit")).splitlines()[0]
        sha = str(commit.get("sha", ""))[:7]
        date = author.get("date", "") if isinstance(author, dict) else ""
        date_label = str(date)[:10] if date else "Date unavailable"
        url = html.escape(str(commit.get("html_url", "#")), quote=True)
        cards.append(
            '<article class="activity-item"><span class="activity-dot"></span><div>'
            f'<a href="{url}">{html.escape(message)}</a>'
            f'<p>{html.escape(date_label)} <span aria-hidden="true">·</span> {html.escape(sha)}</p>'
            '</div></article>'
        )
    if not cards:
        return '<p class="empty-state">Recent commits will appear here when GitHub data is available.</p>'
    return "\n".join(cards)


def render(template: str, counts: dict[str, int], total: int, github: dict[str, object]) -> str:
    stats = github["stats"]
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    error = github["error"]
    notice = (
        f'<p class="notice" role="status">{html.escape(str(error))}</p>'
        if error
        else ""
    )
    branch = stats.get("branch", "main") if isinstance(stats, dict) else "main"
    replacements = {
        "{{GENERATED_AT}}": generated,
        "{{REPOSITORY_URL}}": html.escape(str(github["repository_url"]), quote=True),
        "{{REPOSITORY_NAME}}": html.escape(str(github.get("repository", "Microprocessor Lab Activities"))),
        "{{BRANCH}}": html.escape(str(branch)),
        "{{NOTICE}}": notice,
        "{{STATS}}": format_stats(stats, total),
        "{{TRACKS}}": format_tracks(counts, total),
        "{{MILESTONES}}": format_milestones(github["milestones"]),
        "{{ACTIVITY}}": format_commits(github["commits"]),
    }
    for marker, value in replacements.items():
        template = template.replace(marker, value)
    return template


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", default=discover_repository(), help="GitHub owner/repository")
    parser.add_argument("--output", type=Path, default=ROOT / "docs" / "index.html")
    args = parser.parse_args()

    counts, total = collect_tracks()
    github = collect_github_data(args.repository, os.environ.get("GITHUB_TOKEN"))
    github["repository"] = args.repository or "Microprocessor Lab Activities"
    page = render(TEMPLATE_PATH.read_text(encoding="utf-8"), counts, total, github)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(page, encoding="utf-8")
    print(f"Dashboard written to {args.output} ({total} source files).")
    if github["error"]:
        print(github["error"], file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
