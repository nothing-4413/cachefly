# Security scope

Cachefly is an educational systems project, not an internet-facing Redis replacement. It does not
implement AUTH, ACLs, TLS, command-level authorization, or tenant isolation.

The native default is `127.0.0.1`. The container listens on `0.0.0.0` internally so Prometheus and
host port forwarding work, while Docker Compose publishes Redis, admin HTTP, and Prometheus only on
host loopback. Keep these defaults unless access is restricted by a trusted private network and an
authenticated TLS proxy or equivalent firewall policy.

The admin endpoints expose runtime metrics and effective configuration without authentication. Do
not expose port 8080 to untrusted clients. Persistence files may contain complete keys and values;
protect the data directory with operating-system permissions and storage encryption as appropriate.

Report vulnerabilities through a private GitHub security advisory rather than a public issue.
