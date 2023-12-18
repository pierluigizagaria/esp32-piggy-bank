// @ts-nocheck
import { render } from "preact";
import React, { useEffect, useMemo, useState } from "preact/compat";

import dayjs from "dayjs";
import Papa from "papaparse";

import CssBaseline from "@mui/material/CssBaseline";
import Badge from "@mui/material/Badge";
import { AdapterDayjs } from "@mui/x-date-pickers/AdapterDayjs";
import { LocalizationProvider } from "@mui/x-date-pickers/LocalizationProvider";
import { PickersDay } from "@mui/x-date-pickers/PickersDay";
import { DateCalendar } from "@mui/x-date-pickers/DateCalendar";
import { DayCalendarSkeleton } from "@mui/x-date-pickers/DayCalendarSkeleton";

const DayWithDonation = ({
  day,
  outsideCurrentMonth,
  donations = [],
  ...other
}) => {
  const isSelected =
    !outsideCurrentMonth &&
    donations.find((donation) => dayjs.unix(donation[1]).isSame(day, "day"));
  return (
    <Badge
      key={day.toString()}
      overlap="circular"
      anchorOrigin={{
        vertical: "bottom",
        horizontal: "right",
      }}
      badgeContent={isSelected ? "🟡" : undefined}
    >
      <PickersDay
        {...other}
        outsideCurrentMonth={outsideCurrentMonth}
        day={day}
      />
    </Badge>
  );
};

const DonationCalendar = () => {
  const [currentDay] = useState(dayjs());
  const [selectedDay, setSelectedDay] = useState(dayjs());
  const [isLoading, setIsLoading] = useState(true);
  const [donations, setDonations] = useState([]);

  const fetchDonations = () => {
    Papa.parse("donations.csv", {
      download: true,
      complete: ({ data }) => {
        setIsLoading(false);
        setDonations(data);
      },
      error: () => setIsLoading(false),
    });
  };

  useEffect(() => fetchDonations(), []);

  const getMonthDonations = (donations, day) => {
    return (
      donations?.filter((donation) =>
        dayjs.unix(donation[1]).isSame(day, "month")
      ) ?? []
    );
  };

  const currentMonthDonations = useMemo(() => {
    return getMonthDonations(donations, selectedDay).reduce(
      (sum, a) => sum + parseFloat(a[0]),
      0
    );
  }, [donations, selectedDay]);

  return (
    <LocalizationProvider dateAdapter={AdapterDayjs}>
      <div style={{ display: "flex", justifyContent: "center", marginTop: 16 }}>
        <span style={{ fontSize: 32 }}>
          {`💰 ${currentMonthDonations.toFixed(2)}`}
        </span>
      </div>
      <DateCalendar
        readOnly
        defaultValue={currentDay}
        maxDate={currentDay}
        loading={isLoading}
        onMonthChange={(day) => setSelectedDay(day)}
        renderLoading={() => <DayCalendarSkeleton />}
        slots={{ day: DayWithDonation }}
        slotProps={{ day: { donations } }}
      />
    </LocalizationProvider>
  );
};

export function App() {
  return (
    <React.Fragment>
      <CssBaseline enableColorScheme />
      <DonationCalendar />
    </React.Fragment>
  );
}

render(<App />, document.getElementById("app"));
