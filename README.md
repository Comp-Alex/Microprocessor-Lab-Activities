# Microprocessor Lab Activities

A hands-on collection of microprocessor and embedded systems activities, from small I/O experiments to sensor-driven robotics.

## Engineering dashboard

The dashboard summarizes the source-code portfolio, GitHub repository statistics, repository milestones, and recent commits.

- **Dashboard:** [Open the live engineering dashboard](https://comp-alex.github.io/Microprocessor-Lab-Activities/)
- **Tracks:** assembly programming, lab experiments, architecture concepts, and hardware activities
- **Milestones:** configure GitHub repository milestones and link issues to see milestone progress

Track bars represent each track's share of tracked source files; they are not a measure of learner completion. GitHub statistics, milestones, and recent activity are loaded from the repository API when the dashboard is generated.

## Run locally

Requires Python 3.10 or newer; no third-party packages are needed.

```powershell
python dashboard\generate.py
```

The generated page is written to `docs\index.html`. For live repository statistics from a local run, provide a token with public repository read access:

```powershell
$env:GITHUB_TOKEN = "your-token"
python dashboard\generate.py --repository Comp-Alex/Microprocessor-Lab-Activities
```

Without API access, the local page still includes source-file coverage and displays an explanatory notice for unavailable GitHub data.

## GitHub Pages workflow

The workflow builds and deploys the dashboard on pushes to `main`, on a weekly schedule, or through **Actions → Build and deploy engineering dashboard → Run workflow**. To enable it, open **Settings → Pages** and select **GitHub Actions** as the build and deployment source.

## Repository layout

```text
dashboard/
  generate.py           # Standard-library dashboard generator
  templates/
    index.html          # Responsive, self-contained page template
docs/
  index.html            # Generated dashboard output
.github/
  workflows/
    microprocessor-dashboard.yml
```
