'use client';
import { createTheme } from '@mui/material/styles';

// Dense, green-accented, near-black text: the look of a construction-compliance back office.
export const theme = createTheme({
  colorSchemes: { light: true, dark: true },
  palette: { primary: { main: '#2d6b23' }, success: { main: '#0ca30c' }, error: { main: '#d03b3b' }, warning: { main: '#fab219' } },
  shape: { borderRadius: 6 },
  typography: { fontFamily: 'Inter, -apple-system, Segoe UI, Roboto, sans-serif', fontSize: 13, h6: { fontWeight: 700, fontSize: 15 }, subtitle2: { fontWeight: 700 } },
  components: {
    MuiTableCell: { styleOverrides: { root: { padding: '4px 8px' } } },
    MuiChip: { styleOverrides: { root: { fontWeight: 600 } } },
    MuiPaper: { defaultProps: { variant: 'outlined' } },
  },
});
