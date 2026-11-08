import { Link } from "react-router-dom";
import { Arrow } from "@/components/icons";
import { Stamp } from "@/components/ui";

const NotFound = () => (
  <div className="wrap py-20 md:py-28">
    <div className="sheet max-w-xl p-6 md:p-10 relative">
      <div className="absolute -top-4 right-4">
        <Stamp tilt>Return to sender</Stamp>
      </div>
      <p className="kicker">Error 404</p>
      <h1 className="display-2 mt-2">No such address</h1>
      <p className="mt-4 muted">
        There is no page here. If a link on this site brought you here, that link is wrong; tell us on GitHub and we will fix it.
      </p>
      <p className="mt-8 flex flex-wrap gap-3">
        <Link to="/" className="btn btn-primary">
          Home <Arrow />
        </Link>
        <Link to="/docs" className="btn btn-ghost">
          Docs
        </Link>
      </p>
    </div>
  </div>
);

export default NotFound;
