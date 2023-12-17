// @ts-nocheck
import { render } from "preact";
import React, { useEffect, useMemo, useState } from "preact/compat";

import dayjs from "dayjs";
import initSqlJs from "sql.js";

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
    donations.find((donation) => dayjs(donation[2]).isSame(day, "day"));
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
  const [isLoading, setIsLoading] = useState(false);
  const [db, setDb] = useState();

  useEffect(() => initializeSqlite(), []);

  const initializeSqlite = async () => {
    setIsLoading(true);
    const initSqlite = initSqlJs({ locateFile: (file) => `/${file}` });
    const fetchBuffer = fetch("/database.db").then((res) => res.arrayBuffer());
    const [sqlite, buffer] = await Promise.all([initSqlite, fetchBuffer]);
    const db = new sqlite.Database(new Uint8Array(buffer));
    setDb(db);
  };

  const fetchDonations = () => {
    if (!db) return;
    const [{ values }] = db.exec("SELECT * FROM donations");
    setIsLoading(false);
    return values;
  };

  const donations = useMemo(() => fetchDonations(), [db]);

  const getMonthDonations = (donations, day) => {
    return (
      donations?.filter((donation) =>
        dayjs(donation[2]).isSame(day, "month")
      ) ?? []
    );
  };

  const currentMonthDonations = useMemo(() => {
    return getMonthDonations(donations, selectedDay).reduce(
      (sum, a) => sum + a[1],
      0
    );
  }, [donations, selectedDay]);

  return (
    <LocalizationProvider dateAdapter={AdapterDayjs}>
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
      {
        <div style={{ display: "flex", justifyContent: "center" }}>
          <span style={{ fontSize: 16 }}>
            {`💰 ${currentMonthDonations.toFixed(2)}`}
          </span>
        </div>
      }
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
