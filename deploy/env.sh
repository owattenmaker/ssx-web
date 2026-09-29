# Deploy configuration, sourced by deploy/deploy-staged.sh and deploy/deploy.sh (docs/hosting.md "Deploy configuration").
# The host, its folder and the launchd label are not in the repo: they come from deploy/.env.local (git-ignored; start
# from deploy/.env.example). A variable already set in the environment wins over the file, e.g.
#   SSX_HOST=me@otherhost deploy/deploy-staged.sh
# `sh deploy/env.sh` prints the resolved values without deploying anything.
[ -n "${ROOT:-}" ] || ROOT=$(cd "$(dirname "$0")/.." && pwd)
ssx_env_file="$ROOT/deploy/.env.local"
if [ -f "$ssx_env_file" ]; then
  # KEY=value lines (optionally quoted), # comments; the file is parsed, never executed.
  while IFS= read -r ssx_line || [ -n "$ssx_line" ]; do
    case $ssx_line in '' | '#'*) continue ;; esac
    ssx_key=${ssx_line%%=*}; ssx_val=${ssx_line#*=}
    case $ssx_key in '' | *[!A-Za-z0-9_]*) echo "deploy/.env.local: not KEY=value: $ssx_line" >&2; exit 1 ;; esac
    case $ssx_val in \"*\") ssx_val=${ssx_val#\"}; ssx_val=${ssx_val%\"} ;; \'*\') ssx_val=${ssx_val#\'}; ssx_val=${ssx_val%\'} ;; esac
    eval "ssx_set=\${$ssx_key+x}"
    [ -n "$ssx_set" ] || { eval "$ssx_key=\$ssx_val"; export "$ssx_key"; }
  done < "$ssx_env_file"
fi
for ssx_key in SSX_HOST SSX_HOST_ROOT SSX_LABEL; do
  eval "ssx_val=\${$ssx_key:-}"
  [ -n "$ssx_val" ] || { echo "$ssx_key is not set: copy deploy/.env.example to deploy/.env.local and fill it in" >&2; exit 1; }
done
case $0 in
  *env.sh) printf 'SSX_HOST=%s\nSSX_HOST_ROOT=%s  (~/%s/app/web on the host)\nSSX_LABEL=%s  (%s.server, %s.tunnel)\n' \
    "$SSX_HOST" "$SSX_HOST_ROOT" "$SSX_HOST_ROOT" "$SSX_LABEL" "$SSX_LABEL" "$SSX_LABEL" ;;
esac
