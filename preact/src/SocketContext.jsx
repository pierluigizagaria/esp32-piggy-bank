import { createContext } from "preact/compat";

const socket = new WebSocket(`ws://${location.host}/ws`);

const SocketContext = createContext(socket);

export default SocketContext;
