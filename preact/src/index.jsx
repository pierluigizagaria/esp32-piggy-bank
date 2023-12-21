// @ts-nocheck
import React from "preact/compat";
import { render } from "preact";
import CssBaseline from "@mui/material/CssBaseline/CssBaseline";
import ThemeProvider from "@mui/material/styles/ThemeProvider";
import { AdapterDayjs } from "@mui/x-date-pickers/AdapterDayjs/AdapterDayjs";
import { LocalizationProvider } from "@mui/x-date-pickers/LocalizationProvider/LocalizationProvider";

import theme from "./CustomTheme";
import CustomAppBar from "./components/CustomAppBar";
import DonationCalendar from "./components/DonationCalendar";

export function App() {
  return (
    <React.Fragment>
      <CssBaseline enableColorScheme />
      <ThemeProvider theme={theme}>
        <LocalizationProvider dateAdapter={AdapterDayjs}>
          <CustomAppBar />
          <DonationCalendar />
        </LocalizationProvider>
      </ThemeProvider>
    </React.Fragment>
  );
}

render(<App />, document.getElementById("app"));
