// @ts-nocheck
import createTheme from "@mui/material/styles/createTheme";

const themeOptions = {
  palette: {
    mode: "light",
    primary: {
      main: "#bb7f4d",
    },
    secondary: {
      main: "#a7333f",
    },
  },
};

const customTheme = createTheme(themeOptions);

export default customTheme;
