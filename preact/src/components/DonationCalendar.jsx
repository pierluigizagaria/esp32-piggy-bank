// @ts-nocheck
import { useEffect, useMemo, useState, useContext } from "preact/compat";
import dayjs from "dayjs";
import Papa from "papaparse";
import Typography from "@mui/material/Typography/Typography";
import Popover from "@mui/material/Popover/Popover";
import { DateCalendar } from "@mui/x-date-pickers/DateCalendar/DateCalendar";
import { DayCalendarSkeleton } from "@mui/x-date-pickers/DayCalendarSkeleton/DayCalendarSkeleton";

import SocketContext from "../SocketContext";
import DayWithDonation from "./DayWithDonation";

const DonationCalendar = () => {
  const socket = useContext(SocketContext);
  const [currentDay] = useState(dayjs());
  const [selectedMonth, setSelectedMonth] = useState(dayjs());
  const [isLoading, setIsLoading] = useState(true);
  const [donations, setDonations] = useState([]);

  const [focusedDay, setFocusedDay] = useState();
  const [anchorEl, setAnchorEl] = useState();

  useEffect(() => {
    fetchDonations();
    socket.addEventListener("message", onSocketMessage);
    return () => socket.removeEventListener(onSocketMessage);
  }, []);

  const onSocketMessage = (event) => {
    const { type, message } = JSON.parse(event.data);
    if (type === "Notification" && message === "DONATION_RECEIVED") {
      fetchDonations();
      setAnchorEl();
    }
  };

  const fetchDonations = () => {
    setIsLoading(true);
    Papa.parse("donations.csv", {
      download: true,
      complete: ({ data }) => {
        setIsLoading(false);
        setDonations(data);
      },
      error: () => setIsLoading(false),
    });
  };

  const selectedMonthDonations = useMemo(() => {
    return donations
      .filter((donation) =>
        dayjs.unix(donation[1]).isSame(selectedMonth, "month")
      )
      .reduce((sum, a) => sum + parseFloat(a[0]), 0);
  }, [donations, selectedMonth]);

  const selectedDayDonations = useMemo(() => {
    return donations
      .filter((donation) => dayjs.unix(donation[1]).isSame(focusedDay, "day"))
      .reduce((sum, a) => sum + parseFloat(a[0]), 0);
  }, [donations, focusedDay]);

  return (
    <>
      <div
        style={{
          display: "flex",
          justifyContent: "center",
          marginTop: 32,
          marginBottom: 16,
        }}
      >
        <span style={{ fontSize: 48 }}>
          {`💰 ${selectedMonthDonations.toFixed(1)} €`}
        </span>
      </div>
      <DateCalendar
        value={currentDay}
        defaultValue={currentDay}
        maxDate={currentDay}
        loading={isLoading}
        onChange={(day) => setFocusedDay(day)}
        onYearChange={(day) => setSelectedMonth(day)}
        onMonthChange={(day) => setSelectedMonth(day)}
        renderLoading={() => <DayCalendarSkeleton />}
        slots={{ day: DayWithDonation }}
        slotProps={{
          day: {
            donations,
            onClick: (event) => setAnchorEl(event.currentTarget),
          },
        }}
      />
      <Popover
        disableAutoFocus={true}
        open={!!anchorEl && selectedDayDonations > 0}
        anchorEl={anchorEl}
        onClose={() => setAnchorEl()}
        anchorOrigin={{ vertical: "bottom", horizontal: "center" }}
        transformOrigin={{ vertical: "top", horizontal: "center" }}
      >
        <Typography sx={{ p: 1, fontSize: 14 }}>
          {`${selectedDayDonations.toFixed(1)} €`}
        </Typography>
      </Popover>
    </>
  );
};

export default DonationCalendar;
