$branch = git symbolic-ref --quiet --short HEAD
if ($LASTEXITCODE -eq 0) { $branch } else { git rev-parse --short HEAD }
