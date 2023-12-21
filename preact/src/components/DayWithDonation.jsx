// @ts-nocheck
import dayjs from "dayjs";
import { PickersDay } from "@mui/x-date-pickers/PickersDay/PickersDay";
import Badge from "@mui/material/Badge/Badge";

const DayWithDonation = ({
  day,
  outsideCurrentMonth,
  donations = [],
  ...other
}) => {
  const hasDonations = donations.find((donation) =>
    dayjs.unix(donation[1]).isSame(day, "day")
  );
  return (
    <Badge
      key={day.toString()}
      overlap="circular"
      anchorOrigin={{
        vertical: "bottom",
        horizontal: "right",
      }}
      badgeContent={hasDonations && "🟡"}
    >
      <PickersDay
        {...other}
        outsideCurrentMonth={outsideCurrentMonth}
        day={day}
      />
    </Badge>
  );
};

export default DayWithDonation;
