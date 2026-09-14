'use client';
import Link from 'next/link';
import { usePathname } from 'next/navigation';
import { AppBar, Box, Button, Toolbar, Typography } from '@mui/material';

const links = [{ href: '/', label: 'Live' }, { href: '/replay', label: 'Replay' }, { href: '/sessions', label: 'Sessions' }];

export function Nav() {
  const path = usePathname();
  return (
    <AppBar position="static" color="default" elevation={0} sx={{ borderBottom: '1px solid', borderColor: 'divider' }}>
      <Toolbar variant="dense" sx={{ gap: 2 }}>
        <Typography variant="h6" sx={{ color: 'primary.main' }}>Vertical Geofencing</Typography>
        <Typography variant="caption" color="text.secondary" sx={{ display: { xs: 'none', md: 'block' } }}>floor-level clock-ins · simulator</Typography>
        <Box sx={{ flex: 1 }} />
        {links.map((l) => (
          <Button key={l.href} component={Link} href={l.href} size="small" variant={path === l.href ? 'contained' : 'text'} disableElevation>{l.label}</Button>
        ))}
      </Toolbar>
    </AppBar>
  );
}
