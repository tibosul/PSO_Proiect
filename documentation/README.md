# DHCP Server Documentation

This directory contains the IEEE-formatted documentation for the DHCPv4 and DHCPv6 servers.

## Files

- `dhcp_servers_documentation.tex` - LaTeX source file for the documentation
- `dhcp_servers_documentation.pdf` - Compiled PDF document (generated)

## Compiling the Documentation

To compile the LaTeX document into PDF:

```bash
cd documentation
pdflatex dhcp_servers_documentation.tex
pdflatex dhcp_servers_documentation.tex  # Run twice for cross-references
```

### Requirements

You need a LaTeX distribution installed. On Ubuntu/Debian:

```bash
sudo apt-get install texlive-latex-base texlive-latex-extra texlive-fonts-recommended texlive-publishers texlive-science
```

On other systems:
- Windows: Install [MiKTeX](https://miktex.org/) or [TeX Live](https://www.tug.org/texlive/)
- macOS: Install [MacTeX](https://www.tug.org/mactex/)
- Linux: Install TeX Live from your package manager

### Online Compilation

You can also compile this document using Overleaf:

1. Go to [Overleaf](https://www.overleaf.com/)
2. Create a new project from uploaded file
3. Upload `dhcp_servers_documentation.tex`
4. The document will compile automatically

## Document Contents

The documentation covers:

1. **Introduction** - Overview of DHCP and project objectives
2. **DHCPv4 Server Architecture** - Protocol overview, server components, configuration structure, and lease management
3. **DHCPv6 Server Architecture** - Protocol overview, key differences from DHCPv4, prefix delegation, and configuration
4. **Implementation Details** - Build system, logging, and client implementation
5. **Testing and Deployment** - Testing scenarios, verification procedures, and performance considerations
6. **Network Deployment** - Production deployment, security considerations, high availability, and maintenance
7. **Advanced Features** - Static reservations, option handling, and lease renewal
8. **Project Structure and Organization** - Directory structure and organization
9. **Lessons Learned** - Technical insights, protocol complexity, and testing importance
10. **Conclusion** - Summary and future enhancements

## Format

The documentation uses the IEEE conference paper format (`IEEEtran` class) as specified in the template requirements.

## References

The document includes proper citations to:
- RFC 2131 (DHCPv4)
- RFC 8415 (DHCPv6)
- RFC 2132 (DHCP Options)
- RFC 3315 (DHCPv6 - obsoleted)
- RFC 3633 (IPv6 Prefix Delegation)
- RFC 4361 (Node-specific Client Identifiers)
- Related textbooks and resources
