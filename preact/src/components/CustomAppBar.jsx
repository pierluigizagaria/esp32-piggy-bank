// @ts-nocheck
import { useContext, useEffect, useState } from "preact/compat";
import dayjs from "dayjs";
import AppBar from "@mui/material/AppBar/AppBar";
import Toolbar from "@mui/material/Toolbar/Toolbar";
import Typography from "@mui/material/Typography/Typography";
import IconButton from "@mui/material/IconButton/IconButton";
import SyncIcon from "@mui/icons-material/Sync";
import SyncDisabledIcon from "@mui/icons-material/SyncDisabled";

import SocketContext from "../SocketContext";

const CustomAppBar = () => {
  const socket = useContext(SocketContext);
  const [dateTime, setDateTime] = useState();

  const isConnected = socket.readyState === WebSocket.OPEN;

  useEffect(() => {
    socket.addEventListener("message", onSocketMessage);
    return () => socket.removeEventListener(onSocketMessage);
  }, []);

  const onSocketMessage = (event) => {
    const { type, message } = JSON.parse(event.data);
    if (type === "CurrentDateTime") {
      setDateTime(dayjs.unix(message));
    }
  };

  return (
    <AppBar position="static">
      <Toolbar>
        <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
          PIGGY BANK
        </Typography>
        <Typography variant="h6" component="div" sx={{ pr: 1 }}>
          {dateTime && dateTime.format("HH:mm")}
        </Typography>
        <IconButton
          aria-controls="menu-appbar"
          aria-haspopup="true"
          color="inherit"
          disabled={!isConnected}
          onClick={() =>
            socket.send(
              JSON.stringify({
                type: "UpdateDateTime",
                message: String(currentDay.unix()),
              })
            )
          }
        >
          {isConnected ? <SyncIcon /> : <SyncDisabledIcon />}
        </IconButton>
      </Toolbar>
    </AppBar>
  );
};

export default CustomAppBar;
