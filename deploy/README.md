# Deploy media-server to Proxmox LXC

Forgejo Actions builds and tests on the Proxmox `ubuntu-latest` runner, then rsyncs the
`media-server` binary to CT 101 and restarts systemd.

Workflow: [`.forgejo/workflows/deploy.yml`](../.forgejo/workflows/deploy.yml)

## Provisioned target (CT 101)

| Item | Value |
|------|-------|
| VMID | 101 |
| Hostname | `media-server` |
| IP | `192.168.5.111` |
| OS | Debian 12 |
| Proxmox node | `prox` (`192.168.5.2`) |
| Music library | `/mnt/music` (bind → `/mnt/media/music`, SMB via `mnt-media-boot.service`) |
| Binary | `/usr/local/bin/media-server` |
| Data | `/var/lib/media-server/` (owned by `deploy`) |
| Logs | `/var/log/media-server.log` |
| Service | `media-server.service` (enabled; starts after first deploy) |

Deploy is split like your manual flow: **root** installs the binary,
**deploy** restarts the service (passwordless `sudo` for `systemctl restart` only).

## SSH from Proxmox runner host

On `prox` where `forgejo-runner` runs:

```bash
ssh-keygen -t ed25519 -f deploy-media-server -N ""
ssh-copy-id -i deploy-media-server.pub root@192.168.5.111
ssh-copy-id -i deploy-media-server.pub deploy@192.168.5.111
ssh deploy@192.168.5.111 sudo systemctl status media-server
```

The same private key is used for both users in CI.

## Forgejo repository configuration

1. **Settings → Units** — enable Actions for this repository.
2. **Settings → Secrets**:

| Secret | Value |
|--------|-------|
| `DEPLOY_HOST` | `192.168.5.111` |
| `DEPLOY_USER` | `deploy` |
| `SSH_PRIVATE_KEY` | full private key from `deploy-media-server` |

3. **Settings → Variables** (optional):

| Variable | Value | Purpose |
|----------|-------|---------|
| `BINARY_USER` | `root` | user that writes `/usr/local/bin/media-server` |
| `DEPLOY_PATH` | `/usr/local/bin/media-server` | install path on the LXC |

## systemd unit

The committed [media-server.service](media-server.service) matches CT 101:
runs as `deploy`, library at `/mnt/music`, waits for `mnt-media-boot.service`.

If the live unit differs, reconcile on the LXC:

```bash
curl http://127.0.0.1:8080/api/ping
```

## First deploy (dry run)

1. Push to Forgejo with the workflow files.
2. Add secrets/variables above.
3. Open **Actions → Build and deploy to LXC → Run workflow** (`workflow_dispatch`).
4. Confirm `make test` passes, binary lands on the LXC, and the service starts.
5. Check from your LAN:

```bash
curl http://192.168.5.111:8080/api/ping
```

After a successful manual run, pushes to `main` deploy automatically.

## What CI does not handle (v1)

- Creating or resizing the LXC via Proxmox API
- Installing OS packages on each deploy
- TLS or reverse proxy setup
